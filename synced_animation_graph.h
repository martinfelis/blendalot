#pragma once

#include "scene/animation/animation_player.h"
#include "synced_animation_node.h"

#include <cassert>

class Skeleton3D;

class SyncedAnimationGraph : public Node {
	GDCLASS(SyncedAnimationGraph, Node);

private:
	NodePath animation_player_path;
	NodePath skeleton_path;

	GraphEvaluationContext graph_context = {};
	Ref<SyncedAnimationNode> graph_root_node = nullptr;
	AnimationData graph_output;

protected:
	void _notification(int p_what);
	static void _bind_methods();

	/* ---- General settings for animation ---- */
	AnimationMixer::AnimationCallbackModeProcess callback_mode_process = AnimationMixer::ANIMATION_CALLBACK_MODE_PROCESS_IDLE;
	AnimationMixer::AnimationCallbackModeMethod callback_mode_method = AnimationMixer::ANIMATION_CALLBACK_MODE_METHOD_DEFERRED;
	AnimationMixer::AnimationCallbackModeDiscrete callback_mode_discrete = AnimationMixer::ANIMATION_CALLBACK_MODE_DISCRETE_RECESSIVE;

	bool processing = false;
	bool active = true;

public:
	void _process_graph(double p_delta, bool p_update_only = false);
	void _apply_animation_data(const AnimationData &output_data) const;

	void set_active(bool p_active);
	bool is_active() const;

	void set_animation_player(const NodePath &p_path);
	NodePath get_animation_player() const;

	void set_skeleton(const NodePath &p_path);
	NodePath get_skeleton() const;

	void set_graph_root_node(const Ref<SyncedAnimationNode> &p_animation_node);
	Ref<SyncedAnimationNode> get_graph_root_node() const;

	void set_callback_mode_process(AnimationMixer::AnimationCallbackModeProcess p_mode);
	AnimationMixer::AnimationCallbackModeProcess get_callback_mode_process() const;

	void set_callback_mode_method(AnimationMixer::AnimationCallbackModeMethod p_mode);
	AnimationMixer::AnimationCallbackModeMethod get_callback_mode_method() const;

	void set_callback_mode_discrete(AnimationMixer::AnimationCallbackModeDiscrete p_mode);
	AnimationMixer::AnimationCallbackModeDiscrete get_callback_mode_discrete() const;

	GraphEvaluationContext &get_context() {
		return graph_context;
	}

	SyncedAnimationGraph();

private:
	void _set_process(bool p_process, bool p_force = false);

	void _setup_evaluation_context();
	void _cleanup_evaluation_context();

	void _setup_graph();
};
