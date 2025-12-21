#pragma once

#include "scene/animation/animation_player.h"

#include "core/io/resource.h"
#include "scene/3d/skeleton_3d.h"

#include <cassert>

/**
 * @class AnimationData
 * Represents data that is transported via animation connections in the SyncedAnimationGraph.
 *
 * Essentially, it is a hash map for all Animation::Track values that can are sampled from an Animation.
 */
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

struct GraphEvaluationContext {
	AnimationPlayer *animation_player = nullptr;
	Skeleton3D *skeleton_3d = nullptr;
};

/**
 * @class SyncedAnimationNode
 * Base class for all nodes in an SyncedAnimationGraph including BlendTree nodes and StateMachine states.
 */
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
		bool is_synced = false;

		Animation::LoopMode loop_mode = Animation::LOOP_NONE;
		SyncTrack sync_track;
	};
	NodeTimeInfo node_time_info;
	bool active = false;

	StringName name;

	virtual ~SyncedAnimationNode() override = default;
	virtual void initialize(GraphEvaluationContext &context) {}

	virtual void activate_inputs(Vector<Ref<SyncedAnimationNode>> input_nodes) {
		// By default, all inputs nodes are activated.
		for (const Ref<SyncedAnimationNode> &node : input_nodes) {
			node->active = true;
		}
	}
	virtual void calculate_sync_track(Vector<Ref<SyncedAnimationNode>> input_nodes) {
		// By default, use the SyncTrack of the first input.
		if (input_nodes.size() > 0) {
			node_time_info.sync_track = input_nodes[0]->node_time_info.sync_track;
		}
	}
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
	virtual void evaluate(GraphEvaluationContext &context, const Vector<AnimationData *> &input_datas, AnimationData &output_data) {
		// By default, use the AnimationData of the first input.
		if (input_datas.size() > 0) {
			output_data = *input_datas[0];
		}
	}

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

/**
 * @class BlendTreeBuilder
 * Helper class that is used to build runtime blend trees and also to validate connections.
 */
struct BlendTreeBuilder {
	struct NodeConnectionInfo {
		int parent_node_index = -1;
		HashSet<int> input_subtree_node_indices; // Contains all nodes down to the tree leaves that influence this node.
		LocalVector<int> connected_child_node_index_at_port; // Contains for each input port the index of the node that is connected to it.

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

		void apply_node_mapping(const LocalVector<int> &node_index_mapping) {
			// Map connected node indices
			for (unsigned int j = 0; j < connected_child_node_index_at_port.size(); j++) {
				int connected_node_index = connected_child_node_index_at_port[j];
				connected_child_node_index_at_port[j] = node_index_mapping.find(connected_node_index);
			}

			// Map connected subtrees
			HashSet<int> old_indices = input_subtree_node_indices;
			input_subtree_node_indices.clear();
			for (int old_index : old_indices) {
				input_subtree_node_indices.insert(node_index_mapping.find(old_index));
			}
		}
	};

	Vector<Ref<SyncedAnimationNode>> nodes; // All added nodes
	LocalVector<NodeConnectionInfo> node_connection_info;
	Vector<BlendTreeConnection> connections;

	BlendTreeBuilder() {
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

	void sort_nodes_and_references() {
		LocalVector<int> sorted_node_indices = get_sorted_node_indices();

		Vector<Ref<SyncedAnimationNode>> sorted_nodes;
		Vector<NodeConnectionInfo> old_node_connection_info = node_connection_info;
		for (unsigned int i = 0; i < sorted_node_indices.size(); i++) {
			int node_index = sorted_node_indices[i];
			sorted_nodes.push_back(nodes[node_index]);
			node_connection_info[i] = old_node_connection_info[node_index];
		}
		nodes = sorted_nodes;

		for (NodeConnectionInfo &connection_info : node_connection_info) {
			if (connection_info.parent_node_index != -1) {
				connection_info.parent_node_index = sorted_node_indices[connection_info.parent_node_index];
			}
			connection_info.apply_node_mapping(sorted_node_indices);
		}
	}

	LocalVector<int> get_sorted_node_indices() {
		LocalVector<int> result;

		sort_nodes_recursive(0, result);
		result.reverse();

		return result;
	}

	void sort_nodes_recursive(int node_index, LocalVector<int> &result) {
		for (int input_node_index : node_connection_info[node_index].connected_child_node_index_at_port) {
			if (input_node_index >= 0) {
				sort_nodes_recursive(input_node_index, result);
			}
		}
		result.push_back(node_index);
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
	Vector<Ref<SyncedAnimationNode>> nodes;

	struct NodeRuntimeData {
		Vector<Ref<SyncedAnimationNode>> input_nodes;
		Vector<AnimationData *> input_data;
		AnimationData *output_data = nullptr;
	};
	LocalVector<NodeRuntimeData> _node_runtime_data;

	BlendTreeBuilder tree_builder;
	bool tree_initialized = false;

	void setup_tree() {
		nodes.clear();
		_node_runtime_data.clear();

		tree_builder.sort_nodes_and_references();

		// Add nodes and allocate runtime data
		for (int i = 0; i < tree_builder.nodes.size(); i++) {
			const Ref<SyncedAnimationNode> node = tree_builder.nodes[i];
			nodes.push_back(node);

			NodeRuntimeData node_runtime_data;
			for (int ni = 0; ni < node->get_node_input_count(); ni++) {
				node_runtime_data.input_data.push_back(nullptr);
			}

			node_runtime_data.output_data = nullptr;
			_node_runtime_data.push_back(node_runtime_data);
		}

		// Populate runtime data (only now is this.nodes populated to retrieve the nodes)
		for (int i = 0; i < nodes.size(); i++) {
			Ref<SyncedAnimationNode> node = nodes[i];
			NodeRuntimeData &node_runtime_data = _node_runtime_data[i];

			for (int port_index = 0; port_index < node->get_node_input_count(); port_index++) {
				const int connected_node_index = tree_builder.node_connection_info[i].connected_child_node_index_at_port[port_index];
				node_runtime_data.input_nodes.push_back(nodes[connected_node_index]);
			}
		}

		tree_initialized = true;
	}

public:
	Ref<SyncedAnimationNode> get_output_node() const {
		return tree_builder.nodes[0];
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
		if (tree_initialized) {
			print_error("Cannot add node to BlendTree: BlendTree already initialized.");
			return;
		}

		tree_builder.add_node(node);
	}

	bool add_connection(const Ref<SyncedAnimationNode> &source_node, const Ref<SyncedAnimationNode> &target_node, const StringName &target_port_name) {
		if (tree_initialized) {
			print_error("Cannot add connection to BlendTree: BlendTree already initialized.");
			return false;
		}

		return tree_builder.add_connection(source_node, target_node, target_port_name);
	}

	// overrides from SyncedAnimationNode
	void initialize(GraphEvaluationContext &context) override {
		setup_tree();

		for (Ref<SyncedAnimationNode> node : nodes) {
			node->initialize(context);
		}
	}

	void activate_inputs(Vector<Ref<SyncedAnimationNode>> input_nodes) override {
		nodes[0]->active = true;
		for (int i = 0; i < nodes.size(); i++) {
			Ref<SyncedAnimationNode> node = nodes[i];

			if (!node->active) {
				continue;
			}

			const NodeRuntimeData &node_runtime_data = _node_runtime_data[i];
			node->activate_inputs(node_runtime_data.input_nodes);
		}
	}

	void calculate_sync_track(Vector<Ref<SyncedAnimationNode>> input_nodes) override {
		for (int i = nodes.size() - 1; i > 0; i--) {
			Ref<SyncedAnimationNode> node = nodes[i];

			if (!node->active) {
				continue;
			}

			const NodeRuntimeData &node_runtime_data = _node_runtime_data[i];

			node->calculate_sync_track(node_runtime_data.input_nodes);
		}
	}

	void update_time(double p_delta) override {
		nodes[0]->node_time_info.delta = p_delta;
		nodes[0]->node_time_info.position += p_delta;

		for (int i = 1; i < nodes.size(); i++) {
			Ref<SyncedAnimationNode> node = nodes[i];

			if (!node->active) {
				continue;
			}

			Ref<SyncedAnimationNode> node_parent = nodes[tree_builder.node_connection_info[i].parent_node_index];

			if (node->node_time_info.is_synced) {
				node->update_time(node_parent->node_time_info.position);
			} else {
				node->update_time(node_parent->node_time_info.delta);
			}
		}
	}

	void evaluate(GraphEvaluationContext &context, const Vector<AnimationData *> &input_datas, AnimationData &output_data) override {
		for (int i = nodes.size() - 1; i > 0; i--) {
			const Ref<SyncedAnimationNode> &node = nodes[i];

			if (!node->active) {
				continue;
			}

			NodeRuntimeData &node_runtime_data = _node_runtime_data[i];

			if (i == 1) {
				node_runtime_data.output_data = &output_data;
			} else {
				node_runtime_data.output_data = memnew(AnimationData);
			}
			node->evaluate(context, node_runtime_data.input_data, *node_runtime_data.output_data);

			for (int child_index : tree_builder.node_connection_info[i].connected_child_node_index_at_port) {
				memfree(_node_runtime_data[child_index].output_data);
			}
		}
	}
};
