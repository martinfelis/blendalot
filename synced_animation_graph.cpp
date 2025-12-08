#include "synced_animation_graph.h"

#include "core/os/time.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_player.h"

void SyncedAnimationGraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_active", "active"), &SyncedAnimationGraph::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &SyncedAnimationGraph::is_active);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "is_active");

	ClassDB::bind_method(D_METHOD("set_callback_mode_process", "mode"), &SyncedAnimationGraph::set_callback_mode_process);
	ClassDB::bind_method(D_METHOD("get_callback_mode_process"), &SyncedAnimationGraph::get_callback_mode_process);

	ClassDB::bind_method(D_METHOD("set_callback_mode_method", "mode"), &SyncedAnimationGraph::set_callback_mode_method);
	ClassDB::bind_method(D_METHOD("get_callback_mode_method"), &SyncedAnimationGraph::get_callback_mode_method);

	ClassDB::bind_method(D_METHOD("set_animation_player", "animation_player"), &SyncedAnimationGraph::set_animation_player);
	ClassDB::bind_method(D_METHOD("get_animation_player"), &SyncedAnimationGraph::get_animation_player);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "animation_player", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "AnimationPlayer"), "set_animation_player", "get_animation_player");
	ADD_SIGNAL(MethodInfo(SNAME("animation_player_changed")));

	ClassDB::bind_method(D_METHOD("set_skeleton", "skeleton"), &SyncedAnimationGraph::set_skeleton);
	ClassDB::bind_method(D_METHOD("get_skeleton"), &SyncedAnimationGraph::get_skeleton);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "skeleton", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Skeleton3D"), "set_skeleton", "get_skeleton");
	ADD_SIGNAL(MethodInfo(SNAME("skeleton_changed")));
}

void SyncedAnimationGraph::_notification(int p_what) {
	switch (p_what) {
		case Node::NOTIFICATION_READY: {
			_setup_evaluation_context();
			_setup_graph();

			if (active) {
				_set_process(true);
			}
		} break;

		case Node::NOTIFICATION_INTERNAL_PROCESS: {
			if (active) {
				_process_graph(get_process_delta_time());
			}
		} break;

		case Node::NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (active) {
				_process_graph(get_physics_process_delta_time());
			}
		} break;

		case Node::NOTIFICATION_EXIT_TREE: {
			_cleanup_evaluation_context();
			break;
		}

		default: {
			break;
		}
	}
}

void SyncedAnimationGraph::set_active(bool p_active) {
	if (active == p_active) {
		return;
	}

	active = p_active;
	_set_process(processing, true);
}

bool SyncedAnimationGraph::is_active() const {
	return active;
}

void SyncedAnimationGraph::set_callback_mode_process(AnimationMixer::AnimationCallbackModeProcess p_mode) {
	if (callback_mode_process == p_mode) {
		return;
	}

	bool was_active = is_active();
	if (was_active) {
		set_active(false);
	}

	callback_mode_process = p_mode;

	if (was_active) {
		set_active(true);
	}
}

AnimationMixer::AnimationCallbackModeProcess SyncedAnimationGraph::get_callback_mode_process() const {
	return callback_mode_process;
}

void SyncedAnimationGraph::set_callback_mode_method(AnimationMixer::AnimationCallbackModeMethod p_mode) {
	callback_mode_method = p_mode;
	emit_signal(SNAME("mixer_updated"));
}

AnimationMixer::AnimationCallbackModeMethod SyncedAnimationGraph::get_callback_mode_method() const {
	return callback_mode_method;
}

void SyncedAnimationGraph::set_callback_mode_discrete(AnimationMixer::AnimationCallbackModeDiscrete p_mode) {
	callback_mode_discrete = p_mode;
	emit_signal(SNAME("mixer_updated"));
}

AnimationMixer::AnimationCallbackModeDiscrete SyncedAnimationGraph::get_callback_mode_discrete() const {
	return callback_mode_discrete;
}

void SyncedAnimationGraph::set_animation_player(const NodePath &p_path) {
	animation_player_path = p_path;
	if (p_path.is_empty()) {
		//		set_root_node(SceneStringName(path_pp));
		//		while (animation_libraries.size()) {
		//			remove_animation_library(animation_libraries[0].name);
		//		}
	}
	graph_context.animation_player = Object::cast_to<AnimationPlayer>(get_node_or_null(animation_player_path));

	_setup_evaluation_context();
	_setup_graph();

	emit_signal(SNAME("animation_player_changed")); // Needs to unpin AnimationPlayerEditor.
}

NodePath SyncedAnimationGraph::get_animation_player() const {
	return animation_player_path;
}

void SyncedAnimationGraph::set_skeleton(const NodePath &p_path) {
	skeleton_path = p_path;
	if (p_path.is_empty()) {
		//		set_root_node(SceneStringName(path_pp));
		//		while (animation_libraries.size()) {
		//			remove_animation_library(animation_libraries[0].name);
		//		}
	}
	graph_context.skeleton_3d = Object::cast_to<Skeleton3D>(get_node_or_null(skeleton_path));

	_setup_evaluation_context();
	_setup_graph();

	emit_signal(SNAME("skeleton_changed")); // Needs to unpin AnimationPlayerEditor.
}

NodePath SyncedAnimationGraph::get_skeleton() const {
	return skeleton_path;
}

void SyncedAnimationGraph::set_graph_root_node(const Ref<SyncedAnimationNode> &p_animation_node) {
	if (graph_root_node != p_animation_node) {
		graph_root_node = p_animation_node;
		_setup_graph();
	}
}

Ref<SyncedAnimationNode> SyncedAnimationGraph::get_graph_root_node() const {
	return graph_root_node;
}

void SyncedAnimationGraph::_process_graph(double p_delta, bool p_update_only) {
	if (!graph_root_node.is_valid()) {
		return;
	}

	graph_root_node->activate_inputs();
	graph_root_node->calculate_sync_track();
	graph_root_node->update_time(p_delta);
	graph_root_node->evaluate(graph_context, Vector<AnimationData*>(), graph_output);

	_apply_animation_data(graph_output);
}

void SyncedAnimationGraph::_apply_animation_data(const AnimationData& output_data) const {
	for (const KeyValue<Animation::TypeHash, AnimationData::TrackValue *> &K : output_data.track_values) {
		const AnimationData::TrackValue *track_value = K.value;
		switch (track_value->type) {
			case AnimationData::TrackType::TYPE_POSITION_3D: {
				const AnimationData::PositionTrackValue *position_value = static_cast<const AnimationData::PositionTrackValue *>(track_value);

				NodePath path = position_value->track->path;

				if (path.get_subname_count() == 1) {
					int bone_idx = graph_context.skeleton_3d->find_bone(path.get_subname(0));
					if (bone_idx != -1) {
						graph_context.skeleton_3d->set_bone_pose_position(position_value->bone_idx, position_value->position);
					}
				}

				break;
			}
			case AnimationData::TrackType::TYPE_ROTATION_3D: {
				const AnimationData::RotationTrackValue *rotation_value = static_cast<const AnimationData::RotationTrackValue *>(track_value);

				NodePath path = rotation_value->track->path;

				if (path.get_subname_count() == 1) {
					int bone_idx = graph_context.skeleton_3d->find_bone(path.get_subname(0));
					if (bone_idx != -1) {
						graph_context.skeleton_3d->set_bone_pose_rotation(rotation_value->bone_idx, rotation_value->rotation);
					}
				}

				break;
			}
			default: {
				print_line(vformat("Unsupported track type %d", track_value->type));
				break;
			}
		}
	}

	graph_context.skeleton_3d->force_update_all_bone_transforms();
}

void SyncedAnimationGraph::_set_process(bool p_process, bool p_force) {
	if (processing == p_process && !p_force) {
		return;
	}

	set_physics_process_internal(false);
	set_process_internal(true);

	processing = p_process;
}

void SyncedAnimationGraph::_setup_evaluation_context() {
	_cleanup_evaluation_context();

	graph_context.animation_player = Object::cast_to<AnimationPlayer>(get_node_or_null(animation_player_path));
	graph_context.skeleton_3d = Object::cast_to<Skeleton3D>(get_node_or_null(skeleton_path));
}

void SyncedAnimationGraph::_cleanup_evaluation_context() {
	graph_context.animation_player = nullptr;
	graph_context.skeleton_3d = nullptr;
}

void SyncedAnimationGraph::_setup_graph() {
	if (graph_context.animation_player == nullptr || graph_context.skeleton_3d == nullptr || !graph_root_node.is_valid()) {
		return;
	}

	graph_root_node->initialize(graph_context);
}

SyncedAnimationGraph::SyncedAnimationGraph() {
}
