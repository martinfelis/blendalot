#pragma once

#include "core/io/resource.h"
#include "core/profiling/profiling.h"

#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/animation_library.h"
#include "sync_track.h"

#include <cassert>

/**
 * @class AnimationData
 * Represents data that is transported via animation connections in the SyncedAnimationGraph.
 *
 * In general AnimationData objects should be obtained using the AnimationDataAllocator.
 *
 * The class consists of a buffer containing the data and a hashmap that resolves the
 * Animation::TypeHash of an Animation::Track to the corresponding AnimationData::TrackValue
 * block within the buffer.
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
		TrackType type = TYPE_ANIMATION;

		virtual ~TrackValue() = default;

		virtual void blend(const TrackValue &to_value, const float lambda) {
			print_error(vformat("Blending of TrackValue of type %d with TrackValue of type %d not yet implemented.", type, to_value.type));
		}

		virtual bool operator==(const TrackValue &other_value) const {
			print_error(vformat("Comparing TrackValue of type %d with TrackValue of type %d not yet implemented.", type, other_value.type));
			return false;
		}
		bool operator!=(const TrackValue &other_value) const {
			return !(*this == other_value);
		}

		virtual TrackValue *clone() const {
			print_error(vformat("Cannot clone TrackValue of type %d: not yet implemented.", type));
			return nullptr;
		}
	};

	struct TransformTrackValue : public TrackValue {
		int bone_idx = -1;

		bool loc_used = false;
		bool rot_used = false;
		bool scale_used = false;
		Vector3 init_loc = Vector3(0, 0, 0);
		Quaternion init_rot = Quaternion(0, 0, 0, 1);
		Vector3 init_scale = Vector3(1, 1, 1);
		Vector3 loc;
		Quaternion rot;
		Vector3 scale;

		TransformTrackValue() { type = TYPE_POSITION_3D; }

		void blend(const TrackValue &to_value, const float lambda) override {
			const TransformTrackValue *to_value_casted = &static_cast<const TransformTrackValue &>(to_value);
			assert(bone_idx == to_value_casted->bone_idx);
			if (loc_used) {
				loc = (1. - lambda) * loc + lambda * to_value_casted->loc;
			}

			if (rot_used) {
				rot = rot.slerp(to_value_casted->rot, lambda);
			}

			if (scale_used) {
				scale = (1. - lambda) * scale + lambda * to_value_casted->scale;
			}
		}

		bool operator==(const TrackValue &other_value) const override {
			if (type != other_value.type) {
				return false;
			}

			const TransformTrackValue *other_value_casted = &static_cast<const TransformTrackValue &>(other_value);
			return bone_idx == other_value_casted->bone_idx && loc == other_value_casted->loc && rot == other_value_casted->rot && scale == other_value_casted->scale;
		}
	};

	AnimationData() = default;
	~AnimationData() = default;

	AnimationData(const AnimationData &other) {
		value_buffer_offset = other.value_buffer_offset;
		buffer = other.buffer;
	}
	AnimationData(AnimationData &&other) noexcept :
			// We skip copying the offset as that should be identical for all nodes within a BLTAnimationGraph.
			// value_buffer_offset(std::exchange(other.value_buffer_offset, AHashMap<Animation::TypeHash, size_t, HashHasher>())),
			buffer(std::exchange(other.buffer, LocalVector<uint8_t>())) {
	}
	AnimationData &operator=(const AnimationData &other) {
		AnimationData temp(other);
		std::swap(value_buffer_offset, temp.value_buffer_offset);
		std::swap(buffer, temp.buffer);
		return *this;
	}
	AnimationData &operator=(AnimationData &&other) noexcept {
		std::swap(value_buffer_offset, other.value_buffer_offset);
		std::swap(buffer, other.buffer);
		return *this;
	}

	void allocate_track_value(const Animation::Track *animation_track, const Skeleton3D *skeleton_3d);
	void allocate_track_values(const Ref<Animation> &animation, const Skeleton3D *skeleton_3d);

	template <typename TrackValueType>
	TrackValueType *get_value(const Animation::TypeHash &thash) {
		return reinterpret_cast<TrackValueType *>(&buffer[value_buffer_offset[thash]]);
	}

	template <typename TrackValueType>
	const TrackValueType *get_value(const Animation::TypeHash &thash) const {
		return reinterpret_cast<const TrackValueType *>(&buffer[value_buffer_offset[thash]]);
	}

	bool has_same_tracks(const AnimationData &other) const {
		HashSet<Animation::TypeHash> valid_track_hashes;
		for (const KeyValue<Animation::TypeHash, size_t> &K : value_buffer_offset) {
			valid_track_hashes.insert(K.key);
		}

		for (const KeyValue<Animation::TypeHash, size_t> &K : other.value_buffer_offset) {
			if (HashSet<Animation::TypeHash>::Iterator entry = valid_track_hashes.find(K.key)) {
				valid_track_hashes.remove(entry);
			} else {
				return false;
			}
		}

		return valid_track_hashes.size() == 0;
	}

	void blend(const AnimationData &to_data, const float lambda) {
		GodotProfileZone("AnimationData::blend");

		if (!has_same_tracks(to_data)) {
			print_error("Cannot blend AnimationData: tracks do not match.");
			return;
		}

		for (const KeyValue<Animation::TypeHash, size_t> &K : value_buffer_offset) {
			TrackValue *track_value = get_value<TrackValue>(K.key);
			const TrackValue *other_track_value = to_data.get_value<TrackValue>(K.key);

			track_value->blend(*other_track_value, lambda);
		}
	}

	void sample_from_animation(const Ref<Animation> &animation, const Skeleton3D *skeleton_3d, double p_time);

	AHashMap<Animation::TypeHash, size_t, HashHasher> value_buffer_offset;
	LocalVector<uint8_t> buffer;
};

/**
 * @class AnimationDataAllocator
 *
 * Allows reusing of already allocated AnimationData objects. Stores the default values for all
 * tracks. An allocated AnimationData object always has a resetted state where all TrackValues
 * have the default value.
 *
 * During SyncedAnimationGraph initialization all nodes that generate values for AnimationData
 * must register their tracks in the AnimationDataAllocator to ensure all allocated AnimationData
 * have corresponding tracks.
 */
class AnimationDataAllocator {
	AnimationData default_data;
	List<AnimationData *> allocated_data;

public:
	~AnimationDataAllocator() {
		while (!allocated_data.is_empty()) {
			memfree(allocated_data.front()->get());
			allocated_data.pop_front();
		}
	}

	/// @brief Registers all animation track values for the default_data value.
	void register_track_values(const Ref<Animation> &animation, const Skeleton3D *skeleton_3d);

	AnimationData *allocate() {
		GodotProfileZone("AnimationDataAllocator::allocate_template");
		if (!allocated_data.is_empty()) {
			GodotProfileZone("AnimationDataAllocator::allocate_from_list");
			AnimationData *result = allocated_data.front()->get();
			allocated_data.pop_front();

			// We copy the whole block as the assignment operator copies entries element wise.
			memcpy(result->buffer.ptr(), default_data.buffer.ptr(), default_data.buffer.size());

			return result;
		}

		AnimationData *result = memnew(AnimationData);
		*result = default_data;
		return result;
	}

	void free(AnimationData *data) {
		allocated_data.push_front(data);
	}
};

struct GraphEvaluationContext {
	AnimationPlayer *animation_player = nullptr;
	Skeleton3D *skeleton_3d = nullptr;
	AnimationDataAllocator animation_data_allocator;
};

/**
 * @class BLTAnimationNode
 * Base class for all nodes in an SyncedAnimationGraph including BlendTree nodes and StateMachine states.
 */
class BLTAnimationNode : public Resource {
	GDCLASS(BLTAnimationNode, Resource);

	friend class BLTAnimationGraph;

protected:
	static void _bind_methods();

	virtual void get_parameter_list(List<PropertyInfo> *r_list) const;
	virtual Variant get_parameter_default_value(const StringName &p_parameter) const;
	virtual bool is_parameter_read_only(const StringName &p_parameter) const;

	virtual void set_parameter(const StringName &p_name, const Variant &p_value);
	virtual Variant get_parameter(const StringName &p_name) const;

	virtual void _node_changed();
	virtual void _animation_node_renamed(const ObjectID &p_oid, const String &p_old_name, const String &p_new_name);
	virtual void _animation_node_removed(const ObjectID &p_oid, const StringName &p_node);

public:
	struct NodeTimeInfo {
		double delta = 0.0;
		double position = 0.0;
		double sync_position = 0.0;
		bool is_synced = false;

		Animation::LoopMode loop_mode = Animation::LOOP_NONE;
		SyncTrack sync_track;
	};
	NodeTimeInfo node_time_info;
	bool active = false;

	Vector2 position;

	virtual ~BLTAnimationNode() override = default;
	virtual bool initialize(GraphEvaluationContext &context) {
		node_time_info = {};
		return true;
	}

	virtual void activate_inputs(const Vector<Ref<BLTAnimationNode>> &input_nodes) {
		// By default, all inputs nodes are activated.
		for (const Ref<BLTAnimationNode> &node : input_nodes) {
			if (node.ptr() == nullptr) {
				// TODO: add checking whether tree can be evaluated, i.e. whether all inputs are properly connected.
				continue;
			}

			node->active = true;
			node->node_time_info.is_synced = node_time_info.is_synced;
		}
	}
	virtual void calculate_sync_track(const Vector<Ref<BLTAnimationNode>> &input_nodes) {
		// By default, use the SyncTrack of the first input.
		if (input_nodes.size() > 0) {
			node_time_info.sync_track = input_nodes[0]->node_time_info.sync_track;
			node_time_info.loop_mode = input_nodes[0]->node_time_info.loop_mode;
		}
	}

	virtual void update_time(double p_time) {
		if (node_time_info.is_synced) {
			node_time_info.sync_position = p_time;
		} else {
			node_time_info.delta = p_time;
			node_time_info.position += p_time;
		}
	}

	virtual void evaluate(GraphEvaluationContext &context, const LocalVector<AnimationData *> &input_datas, AnimationData &output_data) {
		GodotProfileZone("AnimationNode::evaluate");
		// By default, use the AnimationData of the first input.
		if (input_datas.size() > 0) {
			output_data = std::move(*input_datas[0]);
		}
	}

	void set_position(const Vector2 &p_position) {
		position = p_position;
	}

	Vector2 get_position() const {
		return position;
	}

	virtual Vector<StringName> get_input_names() const { return {}; }

	TypedArray<StringName> get_input_names_as_typed_array() const {
		TypedArray<StringName> typed_arr;
		Vector<StringName> vec = get_input_names();
		typed_arr.resize(vec.size());
		for (uint32_t i = 0; i < vec.size(); i++) {
			typed_arr[i] = vec[i];
		}
		return typed_arr;
	}

	int get_input_index(const StringName &port_name) const {
		Vector<StringName> inputs = get_input_names();
		return inputs.find(port_name);
	}
	int get_input_count() const {
		Vector<StringName> inputs = get_input_names();
		return inputs.size();
	}

	// Creates a list of nodes nested within the current node. E.g. all nodes within a BlendTree node.
	virtual void get_child_nodes(List<Ref<BLTAnimationNode>> *r_child_nodes) const {}
};

class BLTAnimationNodeSampler : public BLTAnimationNode {
	GDCLASS(BLTAnimationNodeSampler, BLTAnimationNode);

public:
	StringName animation_name;
	AnimationPlayer *animation_player = nullptr;

	void set_animation_player(AnimationPlayer *p_player);
	bool set_animation(const StringName &p_name);
	StringName get_animation() const;
	AnimationPlayer *get_animation_player() const;

	TypedArray<StringName> get_animations_as_typed_array() const;

private:
	Ref<Animation> animation;

	bool initialize(GraphEvaluationContext &context) override;
	void update_time(double p_time) override;
	void evaluate(GraphEvaluationContext &context, const LocalVector<AnimationData *> &inputs, AnimationData &output) override;

protected:
	static void _bind_methods();
};

class BLTAnimationNodeTimeScale : public BLTAnimationNode {
	GDCLASS(BLTAnimationNodeTimeScale, BLTAnimationNode);

public:
	float scale = 1.0f;

private:
	Ref<Animation> animation;

	Vector<StringName> get_input_names() const override {
		return { "Input" };
	}

	bool initialize(GraphEvaluationContext &context) override {
		node_time_info = {};
		// TODO: it should not be necessary to force looping here.		node_time_info.loop_mode = Animation::LOOP_LINEAR;
		return true;
	}
	void calculate_sync_track(const Vector<Ref<BLTAnimationNode>> &input_nodes) override {
		if (node_time_info.is_synced) {
			node_time_info.sync_track = input_nodes[0]->node_time_info.sync_track;
			node_time_info.sync_track.duration *= scale;
		}
	}
	void update_time(double p_time) override {
		if (node_time_info.is_synced) {
			return;
		}

		BLTAnimationNode::update_time(p_time * scale);
	}

protected:
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _get(const StringName &p_name, Variant &r_value) const;
	bool _set(const StringName &p_name, const Variant &p_value);

private:
	StringName scale_name = PNAME("scale");
};

class BLTAnimationNodeOutput : public BLTAnimationNode {
	GDCLASS(BLTAnimationNodeOutput, BLTAnimationNode);

public:
	Vector<StringName> get_input_names() const override {
		return { "Output" };
	}
};

class BLTAnimationNodeBlend2 : public BLTAnimationNode {
	GDCLASS(BLTAnimationNodeBlend2, BLTAnimationNode);

public:
	float blend_weight = 0.0f;
	bool sync = true;

	Vector<StringName> get_input_names() const override {
		return { "Input0", "Input1" };
	}

	bool initialize(GraphEvaluationContext &context) override {
		if (!BLTAnimationNode::initialize(context)) {
			return false;
		}

		if (sync) {
			// TODO: do we always want looping in this case or do we traverse the graph to check what's reasonable?
			node_time_info.loop_mode = Animation::LOOP_LINEAR;
		}

		if (node_time_info.loop_mode != Animation::LOOP_LINEAR) {
			print_line(vformat("Forcing loop mode to linear on nonde %s", get_name()));
			node_time_info.loop_mode = Animation::LOOP_LINEAR;
		}

		return true;
	}
	void activate_inputs(const Vector<Ref<BLTAnimationNode>> &input_nodes) override {
		input_nodes[0]->active = !Math::is_zero_approx(1. - blend_weight);
		input_nodes[1]->active = !Math::is_zero_approx(blend_weight);

		// If this Blend2 node is already synced then inputs are also synced. Otherwise, inputs are only set to synced if synced blending is active in this node.
		input_nodes[0]->node_time_info.is_synced = node_time_info.is_synced || sync;
		input_nodes[1]->node_time_info.is_synced = node_time_info.is_synced || sync;
	}

	void calculate_sync_track(const Vector<Ref<BLTAnimationNode>> &input_nodes) override {
		if (node_time_info.is_synced || sync) {
			// TODO: figure out whether we need to enforce looping mode when syncing is enabled.

			if (Math::is_zero_approx(blend_weight)) {
				node_time_info.sync_track = input_nodes[0]->node_time_info.sync_track;
			} else if (Math::is_zero_approx(blend_weight)) {
				node_time_info.sync_track = input_nodes[1]->node_time_info.sync_track;
			} else {
				node_time_info.sync_track = SyncTrack::blend(blend_weight, input_nodes[0]->node_time_info.sync_track, input_nodes[1]->node_time_info.sync_track);
			}

			// We have to recalculate the current time position from the previous sync position as the blended SyncTrack
			// may not match to the previous time position (e.g. when the current time position is > blended SyncTrack duration).
			node_time_info.position = node_time_info.sync_track.calc_ratio_from_sync_time(node_time_info.sync_position) * node_time_info.sync_track.duration;
		}
	}

	void update_time(double p_delta) override {
		BLTAnimationNode::update_time(p_delta);

		if (sync && !node_time_info.is_synced) {
			if (node_time_info.loop_mode != Animation::LOOP_NONE) {
				if (node_time_info.loop_mode == Animation::LOOP_LINEAR) {
					if (!Math::is_zero_approx(node_time_info.sync_track.duration)) {
						node_time_info.position = Math::fposmod(static_cast<float>(node_time_info.position), node_time_info.sync_track.duration);
						node_time_info.sync_position = node_time_info.sync_track.calc_sync_from_abs_time(node_time_info.position);
					}
				} else {
					assert(false && !"Loop mode ping-pong not yet supported");
				}
			}
		}
	}
	void evaluate(GraphEvaluationContext &context, const LocalVector<AnimationData *> &inputs, AnimationData &output) override;

	void set_use_sync(bool p_sync);
	bool is_using_sync() const;

protected:
	static void _bind_methods();

	void get_parameter_list(List<PropertyInfo> *p_list) const override;
	Variant get_parameter_default_value(const StringName &p_parameter) const override;
	void set_parameter(const StringName &p_name, const Variant &p_value) override;
	Variant get_parameter(const StringName &p_name) const override;

	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _get(const StringName &p_name, Variant &r_value) const;
	bool _set(const StringName &p_name, const Variant &p_value);

private:
	StringName blend_weight_pname = PNAME("blend_amount");
	StringName sync_pname = PNAME("sync");
};

struct BLTBlendTreeConnection {
	Ref<BLTAnimationNode> source_node = nullptr;
	Ref<BLTAnimationNode> target_node = nullptr;
	StringName target_port_name = "";
};

class BLTAnimationNodeBlendTree : public BLTAnimationNode {
	GDCLASS(BLTAnimationNodeBlendTree, BLTAnimationNode);

public:
	enum ConnectionError {
		CONNECTION_OK,
		CONNECTION_ERROR_GRAPH_ALREADY_INITIALIZED,
		CONNECTION_ERROR_NO_SOURCE_NODE,
		CONNECTION_ERROR_NO_TARGET_NODE,
		CONNECTION_ERROR_PARENT_EXISTS,
		CONNECTION_ERROR_TARGET_PORT_NOT_FOUND,
		CONNECTION_ERROR_TARGET_PORT_ALREADY_CONNECTED,
		CONNECTION_ERROR_CONNECTION_CREATES_LOOP,
		CONNECTION_ERROR_CONNECTION_NOT_FOUND
	};

	/**
	 * @class BLTBlendTreeGraph
	 * Helper class that is used to build runtime blend trees and also to validate connections.
	 */
	struct BLTBlendTreeGraph {
		struct NodeConnectionInfo {
			int parent_node_index = -1;
			HashSet<int> input_subtree_node_indices; // Contains all nodes down to the tree leaves that influence this node.
			LocalVector<int> connected_child_node_index_at_port; // Contains for each input port the index of the node that is connected to it.

			NodeConnectionInfo() = default;

			explicit NodeConnectionInfo(const BLTAnimationNode *node) {
				parent_node_index = -1;
				for (int i = 0; i < node->get_input_count(); i++) {
					connected_child_node_index_at_port.push_back(-1);
				}
			}

			void apply_node_mapping(const LocalVector<int> &old_to_new_mapping) {
				// Map connected node indices
				for (unsigned int j = 0; j < connected_child_node_index_at_port.size(); j++) {
					int connected_node_index = connected_child_node_index_at_port[j];
					connected_child_node_index_at_port[j] = connected_node_index == -1 ? -1 : old_to_new_mapping[connected_node_index];
				}

				// Map connected subtrees
				HashSet<int> old_indices = input_subtree_node_indices;
				input_subtree_node_indices.clear();
				for (int old_index : old_indices) {
					input_subtree_node_indices.insert(old_index == -1 ? -1 : old_to_new_mapping[old_index]);
				}
			}

			void _print_subtree() const {
				String result = vformat("subtree node indices #%d: ", input_subtree_node_indices.size());
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

		LocalVector<Ref<BLTAnimationNode>> nodes; // All added nodes
		LocalVector<NodeConnectionInfo> node_connection_info;
		LocalVector<BLTBlendTreeConnection> connections;

		BLTBlendTreeGraph();

		Ref<BLTAnimationNode> get_output_node();
		int find_node_index(const Ref<BLTAnimationNode> &node) const;
		int find_node_index_by_name(const StringName &name) const;
		void _print_graph() const;
		void _print_graph_timeinfo() const;

		void sort_nodes_and_references();
		LocalVector<int> get_sorted_node_indices();
		void sort_nodes_recursive(int node_index, LocalVector<int> &result);
		void add_index_and_update_subtrees_recursive(int node_index, int node_parent_index);
		void remove_subtree_and_update_subtrees_recursive(int node, const HashSet<int> &removed_subtree_indices);

		void add_node(const Ref<BLTAnimationNode> &node);
		bool remove_node(const Ref<BLTAnimationNode> &node);

		ConnectionError is_connection_valid(const Ref<BLTAnimationNode> &source_node, const Ref<BLTAnimationNode> &target_node, StringName target_port_name) const;
		ConnectionError add_connection(const Ref<BLTAnimationNode> &source_node, const Ref<BLTAnimationNode> &target_node, const StringName &target_port_name);
		int find_connection_index(const Ref<BLTAnimationNode> &source_node, const Ref<BLTAnimationNode> &target_node, const StringName &target_port_name) const;
		ConnectionError remove_connection(const Ref<BLTAnimationNode> &source_node, const Ref<BLTAnimationNode> &target_node, const StringName &target_port_name);
	};

private:
	BLTBlendTreeGraph tree_graph;
	bool tree_initialized = false;
	bool is_evaluation_order_dirty = true;
	GraphEvaluationContext *_graph_evaluation_context = nullptr;

	void sort_nodes() {
		_node_runtime_data.clear();
		tree_graph.sort_nodes_and_references();
	}

	void setup_runtime_data() {
		// Add nodes and allocate runtime data
		for (uint32_t i = 0; i < tree_graph.nodes.size(); i++) {
			const Ref<BLTAnimationNode> node = tree_graph.nodes[i];

			NodeRuntimeData node_runtime_data;
			for (int ni = 0; ni < node->get_input_count(); ni++) {
				node_runtime_data.input_data.push_back(nullptr);
			}

			node_runtime_data.output_data = nullptr;
			_node_runtime_data.push_back(node_runtime_data);
		}

		// Populate runtime data (only now is this.nodes populated to retrieve the nodes)
		for (uint32_t i = 0; i < tree_graph.nodes.size(); i++) {
			Ref<BLTAnimationNode> node = tree_graph.nodes[i];
			NodeRuntimeData &node_runtime_data = _node_runtime_data[i];

			for (int port_index = 0; port_index < node->get_input_count(); port_index++) {
				const int connected_node_index = tree_graph.node_connection_info[i].connected_child_node_index_at_port[port_index];
				if (connected_node_index == -1) {
					node_runtime_data.input_nodes.push_back(nullptr);
				} else {
					node_runtime_data.input_nodes.push_back(tree_graph.nodes[connected_node_index]);
				}
			}
		}
	}

	void update_node_evaluation_order() {
		sort_nodes();
		setup_runtime_data();

		is_evaluation_order_dirty = false;
	}

protected:
	static void _bind_methods();
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _get(const StringName &p_name, Variant &r_value) const;
	bool _set(const StringName &p_name, const Variant &p_value);

public:
	Vector2 graph_offset;

	struct NodeRuntimeData {
		Vector<Ref<BLTAnimationNode>> input_nodes;
		LocalVector<AnimationData *> input_data;
		AnimationData *output_data = nullptr;
	};
	LocalVector<NodeRuntimeData> _node_runtime_data;

	void set_graph_offset(const Vector2 &p_graph_offset) {
		graph_offset = p_graph_offset;
	}

	Vector2 get_graph_offset() const {
		return graph_offset;
	}

	int find_node_index(const Ref<BLTAnimationNode> &node) const {
		return tree_graph.find_node_index(node);
	}

	int find_node_index_by_name(const StringName &p_name) const {
		return tree_graph.find_node_index_by_name(p_name);
	}

	void add_node(const Ref<BLTAnimationNode> &node) {
		tree_graph.add_node(node);

		if (_graph_evaluation_context != nullptr) {
			node->initialize(*_graph_evaluation_context);
		}
	}

	void remove_node(const Ref<BLTAnimationNode> &node) {
		if (tree_graph.remove_node(node)) {
			_node_changed();
		}
	}

	TypedArray<StringName> get_node_names_as_typed_array() const {
		Vector<StringName> vec;
		for (const Ref<BLTAnimationNode> &node : tree_graph.nodes) {
			vec.push_back(node->get_name());
		}

		TypedArray<StringName> typed_arr;
		typed_arr.resize(vec.size());
		for (uint32_t i = 0; i < vec.size(); i++) {
			typed_arr[i] = vec[i];
		}
		return typed_arr;
	}

	Ref<BLTAnimationNode> get_node(const StringName &node_name) const {
		int node_index = tree_graph.find_node_index_by_name(node_name);

		if (node_index >= 0) {
			return tree_graph.nodes[node_index];
		}

		return nullptr;
	}

	Ref<BLTAnimationNode> get_node_by_index(int node_index) const {
		if (node_index < 0 || node_index > static_cast<int>(tree_graph.nodes.size())) {
			return nullptr;
		}

		return tree_graph.nodes[node_index];
	}

	Ref<BLTAnimationNode> get_output_node() const {
		return tree_graph.nodes[0];
	}

	ConnectionError is_connection_valid(const Ref<BLTAnimationNode> &source_node, const Ref<BLTAnimationNode> &target_node, const StringName &target_port_name) {
		return tree_graph.is_connection_valid(source_node, target_node, target_port_name);
	}

	ConnectionError add_connection(const Ref<BLTAnimationNode> &source_node, const Ref<BLTAnimationNode> &target_node, const StringName &target_port_name) {
		ConnectionError result = tree_graph.add_connection(source_node, target_node, target_port_name);
		if (result == CONNECTION_OK) {
			is_evaluation_order_dirty = true;
			_node_changed();
		}

		return result;
	}

	ConnectionError remove_connection(const Ref<BLTAnimationNode> &source_node, const Ref<BLTAnimationNode> &target_node, const StringName &target_port_name) {
		ConnectionError result = tree_graph.remove_connection(source_node, target_node, target_port_name);
		if (result == CONNECTION_OK) {
			is_evaluation_order_dirty = true;
			_node_changed();
		}

		return result;
	}

	Array get_connections_as_array() const {
		Array result;
		for (const BLTBlendTreeConnection &connection : tree_graph.connections) {
			result.push_back(connection.source_node);
			result.push_back(connection.target_node);
			result.push_back(connection.target_port_name);
		}

		return result;
	}

	void _tree_node_changed(const StringName &node_name) {
		_node_changed();
	}

	// overrides from BLTAnimationNode
	bool initialize(GraphEvaluationContext &context) override {
		tree_initialized = false;

		if (!BLTAnimationNode::initialize(context)) {
			return false;
		}

		_graph_evaluation_context = &context;

		if (is_evaluation_order_dirty) {
			update_node_evaluation_order();
		}

		const HashSet<int> &output_subtree = tree_graph.node_connection_info[0].input_subtree_node_indices;

		for (unsigned int i = 0; i < tree_graph.nodes.size(); i++) {
			const Ref<BLTAnimationNode> &node = tree_graph.nodes[i];

			// Initialize, but skip validation of nodes that are not part of the active tree.
			if (!output_subtree.has(i)) {
				node->initialize(context);
				continue;
			}

			if (!node->initialize(context)) {
				return false;
			}

			const NodeRuntimeData &node_runtime_data = _node_runtime_data[i];

			for (const Ref<BLTAnimationNode> &input_node : node_runtime_data.input_nodes) {
				if (!input_node.is_valid()) {
					return false;
				}
			}
		}

		tree_initialized = true;

		return true;
	}

	void
	activate_inputs(const Vector<Ref<BLTAnimationNode>> &input_nodes) override {
		GodotProfileZone("SyncedBlendTree::activate_inputs");

		// TODO: add checking whether tree can be evaluated, i.e. whether all inputs are properly connected.
		if (tree_graph.nodes.size() == 1) {
			return;
		}

		tree_graph.nodes[0]->active = true;
		tree_graph.nodes[0]->node_time_info.is_synced = node_time_info.is_synced;

		for (uint32_t i = 0; i < tree_graph.nodes.size(); i++) {
			const Ref<BLTAnimationNode> &node = tree_graph.nodes[i];

			if (!node->active) {
				continue;
			}

			const NodeRuntimeData &node_runtime_data = _node_runtime_data[i];
			node->activate_inputs(node_runtime_data.input_nodes);
		}
	}

	void calculate_sync_track(const Vector<Ref<BLTAnimationNode>> &input_nodes) override {
		GodotProfileZone("SyncedBlendTree::calculate_sync_track");
		for (uint32_t i = tree_graph.nodes.size() - 1; i > 0; i--) {
			const Ref<BLTAnimationNode> &node = tree_graph.nodes[i];

			if (!node->active) {
				continue;
			}

			const NodeRuntimeData &node_runtime_data = _node_runtime_data[i];

			node->calculate_sync_track(node_runtime_data.input_nodes);

			if (i == 1) {
				node_time_info = node->node_time_info;
			}
		}
	}

	void update_time(double p_delta) override {
		GodotProfileZone("SyncedBlendTree::update_time");

		BLTAnimationNode::update_time(p_delta);

		tree_graph.nodes[0]->node_time_info.delta = node_time_info.delta;
		tree_graph.nodes[0]->node_time_info.position = node_time_info.position;
		tree_graph.nodes[0]->node_time_info.sync_position = node_time_info.sync_position;

		for (uint32_t i = 1; i < tree_graph.nodes.size(); i++) {
			const Ref<BLTAnimationNode> &node = tree_graph.nodes[i];

			if (!node->active) {
				continue;
			}

			const BLTBlendTreeGraph::NodeConnectionInfo &node_connection_info = tree_graph.node_connection_info[i];
			const Ref<BLTAnimationNode> &node_parent = tree_graph.nodes[node_connection_info.parent_node_index];

			if (node->node_time_info.is_synced) {
				node->update_time(node_parent->node_time_info.sync_position);
			} else {
				node->update_time(node_parent->node_time_info.delta);
			}
		}
	}

	void evaluate(GraphEvaluationContext &context, const LocalVector<AnimationData *> &input_datas, AnimationData &output_data) override {
		GodotProfileZone("SyncedBlendTree::evaluate");

		for (uint32_t i = tree_graph.nodes.size() - 1; i > 0; i--) {
			const Ref<BLTAnimationNode> &node = tree_graph.nodes[i];

			if (!node->active) {
				continue;
			}

			NodeRuntimeData &node_runtime_data = _node_runtime_data[i];

			// Populate the inputs
			for (unsigned int j = 0; j < node_runtime_data.input_data.size(); j++) {
				int child_node_index = tree_graph.node_connection_info[i].connected_child_node_index_at_port[j];
				if (tree_graph.nodes[child_node_index]->active) {
					node_runtime_data.input_data[j] = _node_runtime_data[child_node_index].output_data;
				} else {
					node_runtime_data.input_data[j] = nullptr;
				}
			}

			// Set output pointer
			if (i == 1) {
				// We're emitting to the Output node, therefore we can just copy the data to the output AnimationData.
				node_runtime_data.output_data = &output_data;
				node->active = false;
			} else {
				node_runtime_data.output_data = context.animation_data_allocator.allocate();
			}

			node->evaluate(context, node_runtime_data.input_data, *node_runtime_data.output_data);

			// All inputs have been consumed and can now be freed and nodes deactivated.
			for (unsigned int j = 0; j < node_runtime_data.input_data.size(); j++) {
				int child_node_index = tree_graph.node_connection_info[i].connected_child_node_index_at_port[j];
				if (tree_graph.nodes[child_node_index]->active) {
					context.animation_data_allocator.free(_node_runtime_data[child_node_index].output_data);
					tree_graph.nodes[j]->active = false;
				} else {
					node_runtime_data.input_data[j] = nullptr;
				}
			}
		}
	}

	void get_child_nodes(List<Ref<BLTAnimationNode>> *r_child_nodes) const override {
		for (const Ref<BLTAnimationNode> &node : tree_graph.nodes) {
			r_child_nodes->push_back(node.ptr());
		}
	}
};

VARIANT_ENUM_CAST(BLTAnimationNodeBlendTree::ConnectionError)
