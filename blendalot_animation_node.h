#pragma once

#ifdef BLENDALOT_GDEXTENSION
#include "gdextension_helper.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/skeleton3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/string_name.hpp>

// Hack of a Hack (see comments in Animation::Track::get_unique_id()
typedef uint64_t AnimationTrackUID;

// Disable profiling
#define GodotProfileZone(m_zone_name)
#else
#include "core/io/resource.h"
#include "core/profiling/profiling.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/animation_library.h"

// Hack of a Hack (see comments in Animation::Track::get_unique_id()
typedef Animation::TrackCacheID AnimationTrackUID;
#endif

#include "blendalot_animation_data.h"

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
	StringName node_path;

	Vector2 position;

	virtual ~BLTAnimationNode() override = default;

	/// Validates the node and also the nodes it contains. If node cannot be evaluated this function must add a
	/// message to context.validation_messages and return false.
	virtual bool initialize(GraphEvaluationContext &context) {
		node_time_info = {};
		return true;
	}

	/// Marks a node as active for the current graph evaluation.
	/// This function marks a node as active such that the remaining functions calculate_sync_track(), update_time(),
	/// and evaluate() are called. In addition, this function must also take care of propagating the is_synced flag
	/// to its inputs.
	virtual void activate_inputs(const LocalVector<Ref<BLTAnimationNode>> &input_nodes) {
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

	virtual void calculate_sync_track(const LocalVector<Ref<BLTAnimationNode>> &input_nodes) {
		// By default, use the SyncTrack of the first input.
		if (input_nodes.size() > 0) {
			node_time_info.sync_track = input_nodes[0]->node_time_info.sync_track;
			node_time_info.loop_mode = input_nodes[0]->node_time_info.loop_mode;
		}
	}

	/// Updates the node time, usually from the parent node.
	/// \param p_delta is the time delta change of this BLTAnimationGraph update. For synced nodes this is the time delta of the sync root (e.g. a synced Blend2).
	/// \param p_sync_position is the synced time position when the parent node is synced. For unsynced nodes this parameter is not used.
	virtual void update_time(const double p_delta, const double p_sync_position = 0.0) {
		if (node_time_info.is_synced) {
			node_time_info.delta = p_delta;
			node_time_info.sync_position = p_sync_position;
		} else {
			node_time_info.delta = p_delta;
			node_time_info.position += p_delta;
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
	bool set_animation(const String &p_name);
	StringName get_animation() const;
	AnimationPlayer *get_animation_player() const;

	TypedArray<StringName> get_animations_as_typed_array() const;

private:
	Ref<Animation> animation;

	bool initialize(GraphEvaluationContext &context) override;
	void update_time(const double p_delta, const double p_time = 0.0) override;
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
	void calculate_sync_track(const LocalVector<Ref<BLTAnimationNode>> &input_nodes) override {
		if (node_time_info.is_synced) {
			node_time_info.sync_track = input_nodes[0]->node_time_info.sync_track;
			node_time_info.sync_track.duration *= scale;
		}
	}
	void update_time(const double p_delta, const double p_time = 0.0) override {
		if (node_time_info.is_synced) {
			BLTAnimationNode::update_time(p_delta, p_time);
		}

		BLTAnimationNode::update_time(p_delta * scale);
	}

protected:
	static void _bind_methods() {};

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

protected:
	static void _bind_methods() {};
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
	void activate_inputs(const LocalVector<Ref<BLTAnimationNode>> &input_nodes) override {
		input_nodes[0]->active = !Math::is_zero_approx(1. - blend_weight);
		input_nodes[1]->active = !Math::is_zero_approx(blend_weight);

		// If this Blend2 node is already synced then inputs are also synced. Otherwise, inputs are only set to synced if synced blending is active in this node.
		input_nodes[0]->node_time_info.is_synced = node_time_info.is_synced || sync;
		input_nodes[1]->node_time_info.is_synced = node_time_info.is_synced || sync;
	}

	void calculate_sync_track(const LocalVector<Ref<BLTAnimationNode>> &input_nodes) override {
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

	void update_time(const double p_delta, const double p_time = 0.0) override {
		BLTAnimationNode::update_time(p_delta, p_time);

		if (sync && !node_time_info.is_synced) {
			if (node_time_info.loop_mode != Animation::LOOP_NONE) {
				if (node_time_info.loop_mode == Animation::LOOP_LINEAR) {
					if (!Math::is_zero_approx(node_time_info.sync_track.duration)) {
						node_time_info.position = Math::fposmod(static_cast<float>(node_time_info.position), node_time_info.sync_track.duration);
						node_time_info.sync_position = node_time_info.sync_track.calc_sync_from_abs_time(node_time_info.position);

						// delta stays in the AnimationGraph time domain.
						node_time_info.delta = p_delta;
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
