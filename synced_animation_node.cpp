//
// Created by martin on 03.12.25.
//

#include "synced_animation_node.h"

void AnimationSamplerNode::initialize(GraphEvaluationContext &context) {
	animation = context.animation_player->get_animation(animation_name);
	node_time_info.length = animation->get_length();
	node_time_info.loop_mode = Animation::LOOP_LINEAR;
}

void AnimationSamplerNode::evaluate(GraphEvaluationContext &context, AnimationData &output) {
	const Vector<Animation::Track *> tracks = animation->get_tracks();
	Animation::Track *const *tracks_ptr = tracks.ptr();

	int count = tracks.size();
	for (int i = 0; i < count; i++) {
		AnimationData::TrackValue *track_value = nullptr;
		const Animation::Track *animation_track = tracks_ptr[i];
		const NodePath& track_node_path = animation_track->path;
		if (!animation_track->enabled) {
			continue;
		}

		Animation::TrackType ttype = animation_track->type;
		switch (ttype) {
			case Animation::TYPE_POSITION_3D: {
				AnimationData::PositionTrackValue *position_track_value = memnew(AnimationData::PositionTrackValue);

				if (track_node_path.get_subname_count() == 1) {
					int bone_idx = context.skeleton_3d->find_bone(track_node_path.get_subname(0));
					if (bone_idx != -1) {
						position_track_value->bone_idx = bone_idx;
					}
					animation->try_position_track_interpolate(i, node_time_info.position, &position_track_value->position);
				} else {
					// TODO
					assert(false && !"Not yet implemented");
				}

				track_value = position_track_value;
				break;
			}
			case Animation::TYPE_ROTATION_3D: {
				AnimationData::RotationTrackValue *rotation_track_value = memnew(AnimationData::RotationTrackValue);

				if (track_node_path.get_subname_count() == 1) {
					int bone_idx = context.skeleton_3d->find_bone(track_node_path.get_subname(0));
					if (bone_idx != -1) {
						rotation_track_value->bone_idx = bone_idx;
					}
					animation->try_rotation_track_interpolate(i, node_time_info.position, &rotation_track_value->rotation);
				} else {
					// TODO
					assert(false && !"Not yet implemented");
				}

				track_value = rotation_track_value;
				break;
			}
			default: {
				// TODO
				assert(false && !"Not yet implemented");
				break;
			}
		}

		track_value->track = tracks_ptr[i];
		output.set_value(animation_track->thash, track_value);
	}
}