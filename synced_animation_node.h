#pragma once

#include "scene/animation/animation_player.h"

#include "core/io/resource.h"
#include "scene/3d/skeleton_3d.h"

#include <cassert>

struct GraphEvaluationContext {
	AnimationPlayer *animation_player = nullptr;
	Skeleton3D *skeleton_3d = nullptr;
};

struct AnimationData {
	enum TrackType : uint8_t {
		TYPE_VALUE, // Set a value in a property, can be interpolated.
		TYPE_POSITION_3D, // Position 3D track, can be compressed.
		TYPE_ROTATION_3D, // Rotation 3D track, can be compressed.
		TYPE_SCALE_3D, // Scale 3D track, can be compressed.
		TYPE_BLEND_SHAPE, // Blend Shape track, can be compressed.
		TYPE_METHOD, // Call any method on a specific node.
		TYPE_BEZIER, // Bezier curve.
		TYPE_AUDIO,
		TYPE_ANIMATION,
	};

	struct TrackValue {
		Animation::Track *track = nullptr;
		TrackType type = TYPE_ANIMATION;
	};

	struct PositionTrackValue : public TrackValue {
		int bone_idx = -1;
		Vector3 position = Vector3(0, 0, 0);
		PositionTrackValue() { type = TYPE_POSITION_3D; }
	};

	struct RotationTrackValue : public TrackValue {
		int bone_idx = -1;
		Quaternion rotation = Quaternion(0, 0, 0, 1);
		RotationTrackValue() { type = TYPE_ROTATION_3D; }
	};

	struct ScaleTrackValue : public TrackValue {
		int bone_idx = -1;
		Vector3 scale;
		ScaleTrackValue() { type = TYPE_SCALE_3D; }
	};

	AnimationData() = default;
	~AnimationData() {
		_clear_values();
	}

	void set_value(Animation::TypeHash thash, TrackValue *value) {
		if (!track_values.has(thash)) {
			track_values.insert(thash, value);
		} else {
			track_values[thash] = value;
		}
	}

	void clear() {
		_clear_values();
	}

	AHashMap<Animation::TypeHash, TrackValue *, HashHasher> track_values; // Animation::Track to TrackValue

protected:
	void _clear_values() {
		for (KeyValue<Animation::TypeHash, TrackValue *> &K : track_values) {
			memdelete(K.value);
		}
	}
};

struct SyncTrack {
};

class SyncedAnimationNode : public Resource {
	GDCLASS(SyncedAnimationNode, Resource);

	friend class SyncedAnimationGraph;

public:
	struct NodeTimeInfo {
		double length = 0.0;
		double position = 0.0;
		double sync_position = 0.0;
		double delta = 0.0;
		double sync_delta = 0.0;

		Animation::LoopMode loop_mode = Animation::LOOP_NONE;
		SyncTrack sync_track;
	};
	NodeTimeInfo node_time_info;

	struct InputPort {
		StringName name;
		SyncedAnimationNode *node;
	};

	Vector<InputPort> input_port;

	StringName name;

	virtual ~SyncedAnimationNode() = default;
	virtual void initialize(GraphEvaluationContext &context) {}
	virtual void activate_inputs() {}
	virtual void calculate_sync_track() {}
	virtual void update_time(double p_delta) {
		node_time_info.delta = p_delta;
		node_time_info.position += p_delta;
		if (node_time_info.position > node_time_info.length) {
			switch (node_time_info.loop_mode) {
				case Animation::LOOP_NONE: {
					node_time_info.position = node_time_info.length;
					break;
				}
				case Animation::LOOP_LINEAR: {
					assert(node_time_info.length > 0.0);
					while (node_time_info.position > node_time_info.length) {
						node_time_info.position -= node_time_info.length;
					}
					break;
				}
				case Animation::LOOP_PINGPONG: {
					assert(false && !"Not yet implemented.");
					break;
				}
			}
		}
	}
	virtual void evaluate(GraphEvaluationContext &context, const Vector<AnimationData *> &inputs, AnimationData &output) {}

	bool is_active() const { return active; }
	bool set_input_node(const StringName &socket_name, SyncedAnimationNode *node);
	virtual void get_input_names(Vector<StringName> &inputs) const {}

	int get_node_input_index(const StringName &port_name) const {
		Vector<StringName> inputs;
		get_input_names(inputs);
		return inputs.find(port_name);
	}
	int get_node_input_count() const {
		Vector<StringName> inputs;
		get_input_names(inputs);
		return inputs.size();
	}

private:
	bool active = false;
};

class AnimationSamplerNode : public SyncedAnimationNode {
	GDCLASS(AnimationSamplerNode, SyncedAnimationNode);

public:
	StringName animation_name;

private:
	Ref<Animation> animation;

	void initialize(GraphEvaluationContext &context) override;
	void evaluate(GraphEvaluationContext &context, const Vector<AnimationData *> &inputs, AnimationData &output) override;
};

class OutputNode : public SyncedAnimationNode {
public:
	void get_input_names(Vector<StringName> &inputs) const override {
		inputs.push_back("Input");
	}
};

class AnimationBlend2Node : public SyncedAnimationNode {
public:
	void get_input_names(Vector<StringName> &inputs) const override {
		inputs.push_back("Input0");
		inputs.push_back("Input1");
	}
};

struct BlendTreeConnection {
	const Ref<SyncedAnimationNode> source_node = nullptr;
	const Ref<SyncedAnimationNode> target_node = nullptr;
	const StringName target_port_name = "";
};

struct SortedTreeConstructor {
	struct NodeConnectionInfo {
		int parent_node_index = -1;
		HashSet<int> input_subtree_node_indices;
		LocalVector<int> connected_child_node_index_at_port;

		NodeConnectionInfo() = default;

		explicit NodeConnectionInfo(const SyncedAnimationNode *node) {
			parent_node_index = -1;
			for (int i = 0; i < node->get_node_input_count(); i++) {
				connected_child_node_index_at_port.push_back(-1);
			}
		}

		void _print_subtree() const {
			String result = vformat("subtree node indices (%d): ", input_subtree_node_indices.size());
			bool is_first = true;
			for (int index : input_subtree_node_indices) {
				if (is_first) {
					result += vformat("%d", index);
					is_first = false;
				} else {
					result += vformat(", %d", index);
				}
			}
			print_line(result);
		}
	};

	Vector<Ref<SyncedAnimationNode>> nodes; // All added nodes
	LocalVector<NodeConnectionInfo> node_connection_info;
	Vector<BlendTreeConnection> connections;

	SortedTreeConstructor() {
		Ref<OutputNode> output_node;
		output_node.instantiate();
		output_node->name = "Output";
		add_node(output_node);
	}

	Ref<SyncedAnimationNode> get_output_node() const {
		return nodes[0];
	}

	int get_node_index(const Ref<SyncedAnimationNode> &node) const {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i] == node) {
				return i;
			}
		}

		return -1;
	}

	void add_node(const Ref<SyncedAnimationNode> &node) {
		nodes.push_back(node);
		node_connection_info.push_back(NodeConnectionInfo(node.ptr()));
	}

	void add_index_and_update_subtrees_recursive(int node, int node_parent) {
		if (node_parent == -1) {
			return;
		}

		node_connection_info[node_parent].input_subtree_node_indices.insert(node);

		for (int index : node_connection_info[node].input_subtree_node_indices) {
			node_connection_info[node_parent].input_subtree_node_indices.insert(index);
		}

		add_index_and_update_subtrees_recursive(node_parent, node_connection_info[node_parent].parent_node_index);
	}

	bool add_connection(const Ref<SyncedAnimationNode> &source_node, const Ref<SyncedAnimationNode> &target_node, const StringName &target_port_name) {
		if (!is_connection_valid(source_node, target_node, target_port_name)) {
			return false;
		}

		int source_node_index = get_node_index(source_node);
		int target_node_index = get_node_index(target_node);
		int target_input_port_index = target_node->get_node_input_index(target_port_name);

		node_connection_info[source_node_index].parent_node_index = target_node_index;
		node_connection_info[target_node_index].connected_child_node_index_at_port[target_input_port_index] = source_node_index;

		add_index_and_update_subtrees_recursive(source_node_index, target_node_index);

		return true;
	}

	bool is_connection_valid(const Ref<SyncedAnimationNode> &source_node, const Ref<SyncedAnimationNode> &target_node, StringName target_port_name) {
		int source_node_index = get_node_index(source_node);
		if (source_node_index == -1) {
			print_error("Cannot connect nodes: source node not found.");
			return false;
		}

		if (node_connection_info[source_node_index].parent_node_index != -1) {
			print_error("Cannot connect node: source node already has a parent.");
			return false;
		}

		int target_node_index = get_node_index(target_node);
		if (target_node_index == -1) {
			print_error("Cannot connect nodes: target node not found.");
			return false;
		}

		if (target_node == get_output_node() && connections.size() > 0) {
			print_error("Cannot add connection to output node: output node is already connected");
			return false;
		}

		Vector<StringName> target_inputs;
		target_node->get_input_names(target_inputs);

		if (!target_inputs.has(target_port_name)) {
			print_error("Cannot connect nodes: target port not found.");
			return false;
		}

		int target_input_port_index = target_node->get_node_input_index(target_port_name);
		if (node_connection_info[target_node_index].connected_child_node_index_at_port[target_input_port_index] != -1) {
			print_error("Cannot connect node: target port already connected");
			return false;
		}

		if (node_connection_info[source_node_index].input_subtree_node_indices.has(target_node_index)) {
			print_error("Cannot connect node: connection would create loop.");
			return false;
		}

		return true;
	}
};

class SyncedBlendTree : public SyncedAnimationNode {
	Vector<Ref<SyncedAnimationNode>> tree_nodes;
	Vector<Vector<int>> tree_node_subgraph;

	Vector<BlendTreeConnection> tree_connections;

	Vector<Ref<SyncedAnimationNode>> nodes;
	Vector<int> node_parent_index;
	Vector<Vector<int>> node_subgraph;
	Vector<Vector<Ref<SyncedAnimationNode>>> node_input_nodes;
	Vector<Vector<Ref<AnimationData>>> node_input_data;
	Vector<Ref<AnimationData>> node_output_data;

	void _setup_graph_evaluation() {
		// After this functions we must have:
		// * nodes sorted by evaluation order
		// * node_parent filled
		// * Arrays for node_input_data and node_output_data are filled with empty values
	}

public:
	SyncedBlendTree() {
		Ref<OutputNode> output_node;
		output_node.instantiate();
		output_node->name = "Output";
		nodes.push_back(output_node);
	}

	Ref<SyncedAnimationNode> get_output_node() {
		return nodes[0];
	}

	int get_node_index(const Ref<SyncedAnimationNode> node) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i] == node) {
				return i;
			}
		}

		return -1;
	}

	int add_node(const Ref<SyncedAnimationNode> &node) {
		nodes.push_back(node);
		int node_index = nodes.size() - 1;
		return node_index;
	}

	// overrides from SyncedAnimationNode
	void initialize(GraphEvaluationContext &context) override {
		for (Ref<SyncedAnimationNode> node : nodes) {
			node->initialize(context);
		}
	}

	void activate_inputs() override {
	}

	void calculate_sync_track() override {
	}

	void update_time(double p_delta) override {
	}

	void evaluate(GraphEvaluationContext &context, const Vector<AnimationData *> &inputs, AnimationData &output) override {
	}
};
