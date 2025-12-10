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

class SyncedAnimationNode: public Resource {
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
	virtual void evaluate(GraphEvaluationContext &context, const Vector<AnimationData*>& inputs, AnimationData &output) {}

	bool is_active() const { return active; }
	bool set_input_node(const StringName &socket_name, SyncedAnimationNode *node);
	virtual void get_input_names(Vector<StringName> &inputs) {};

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
	void evaluate(GraphEvaluationContext &context, const Vector<AnimationData*>& inputs, AnimationData &output) override;
};

class OutputNode : public SyncedAnimationNode {
public:
	void get_input_names(Vector<StringName> &inputs) override {
		inputs.push_back("Input");
	}
};

class AnimationBlend2Node : public SyncedAnimationNode {
public:
	void get_input_names(Vector<StringName> &inputs) override {
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
	Vector<HashSet<SyncedAnimationNode*>> node_subgraph;
	Vector<Ref<SyncedAnimationNode>> nodes;
	Vector<BlendTreeConnection> connections;

	SortedTreeConstructor() {
		Ref<OutputNode> output_node;
		output_node.instantiate();
		output_node->name = "Output";
		add_node(output_node);
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

	void add_node(const Ref<SyncedAnimationNode>& node) {
		nodes.push_back(node);
		node_subgraph.push_back(HashSet<SyncedAnimationNode*>());
	}

	bool add_connection(const Ref<SyncedAnimationNode>& source_node, const Ref<SyncedAnimationNode>& target_node, const StringName& target_port_name) {
		if (!is_connection_valid(source_node, target_node, target_port_name)) {
			return false;
		}

		// check for loops
		int source_node_index = get_node_index(source_node);
		if (node_subgraph.get(source_node_index).has(target_node.ptr())) {
			return false;
		}

		int target_node_index = get_node_index(target_node);
		node_subgraph.get(target_node_index).insert(source_node.ptr());

		return true;
	}

	bool is_connection_valid(const Ref<SyncedAnimationNode>& source_node, const Ref<SyncedAnimationNode>& target_node, StringName target_port_name) {
		if (get_node_index(source_node) == -1) {
			print_error("Cannot connect nodes: source node not found.");
			return false;
		}

		if (get_node_index(target_node) == -1) {
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

	int add_node(const Ref<SyncedAnimationNode>& node) {
		nodes.push_back(node);
		int node_index = nodes.size() - 1;
		return node_index;
	}

	bool connect_nodes(const Ref<SyncedAnimationNode>& source_node, const Ref<SyncedAnimationNode>& target_node, StringName target_socket_name) {
		if (!is_connection_valid(source_node, target_node, target_socket_name)) {
			return false;
		}

		return false;
	}

	bool is_connection_valid(const Ref<SyncedAnimationNode>& source_node, const Ref<SyncedAnimationNode>& target_node, StringName target_socket_name) {
		if (get_node_index(source_node) == -1) {
			print_error("Cannot connect nodes: source node not found.");
			return false;
		}

		if (get_node_index(target_node) == -1) {
			print_error("Cannot connect nodes: target node not found.");
			return false;
		}

		if (target_node == get_output_node() && tree_connections.size() > 0) {
			print_error("Cannot add connection to output node: output node is already connected");
			return false;
		}

		Vector<StringName> target_inputs;
		target_node->get_input_names(target_inputs);

		if (!target_inputs.has(target_socket_name)) {
			print_error("Cannot connect nodes: target socket not found.");
			return false;
		}

		return true;
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

	void evaluate(GraphEvaluationContext &context, const Vector<AnimationData*>& inputs, AnimationData &output) override {

	}
};
