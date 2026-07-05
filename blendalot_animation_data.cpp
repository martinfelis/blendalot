#include "blendalot_animation_data.h"

void AnimationData::sample_from_animation(const Ref<Animation> &animation, double p_time, const double delta_time, const int root_bone_track_index) {
	GodotProfileZone("AnimationData::sample_from_animation");

	int count = animation->get_track_count();
	AnimationTrackUID root_bone_track_uid = -1;
	if (root_bone_track_index != -1) {
		root_bone_track_uid = create_track_uid(animation->track_get_path(root_bone_track_index), Animation::TYPE_POSITION_3D);
	}

	for (int32_t track_index = 0; track_index < count; track_index++) {
		if (!animation->track_is_enabled(track_index)) {
			continue;
		}

		Animation::TrackType track_type = animation->track_get_type(track_index);
		AnimationTrackUID track_uid = create_track_uid(animation->track_get_path(track_index), track_type);
		switch (track_type) {
			case Animation::TYPE_POSITION_3D:
			case Animation::TYPE_ROTATION_3D: {
				TransformTrackValue *transform_track_value = get_value_at_index<TransformTrackValue>(get_value_index_from_unique_id(track_uid));

				if (transform_track_value->bone_idx != -1) {
					if (track_uid == root_bone_track_uid) {
						sample_root_bone(animation, track_index, track_type, p_time, delta_time, transform_track_value);
						continue;
					}
					switch (track_type) {
						case Animation::TYPE_POSITION_3D: {
							transform_track_value->loc = animation->position_track_interpolate(track_index, p_time);
							transform_track_value->loc_used = true;
							break;
						}
						case Animation::TYPE_ROTATION_3D: {
							transform_track_value->rot = animation->rotation_track_interpolate(track_index, p_time);
							transform_track_value->rot_used = true;
							break;
						}
						default: {
							assert(false && !"Not yet implemented");
							break;
						}
					}
				} else {
					// TODO
					assert(false && !"Not yet implemented");
				}
				break;
			}
			default: {
				// TODO
				assert(false && !"Not yet implemented");
				break;
			}
		}
	}
}

void AnimationData::sample_root_bone(const Ref<Animation> &animation, const int32_t track_index, const Animation::TrackType track_type, const double &p_time, double delta_time, AnimationData::TransformTrackValue *transform_track_value) {
	bool has_looped = false;
	const double animation_length = animation->get_length();
	double end_remainder = 0.;
	int loop_count = 0;

	if (p_time - delta_time < 0.0) {
		has_looped = true;
		end_remainder = Math::fposmod(animation_length + (p_time - delta_time), animation_length);
		loop_count = Math::floor((delta_time - animation_length) / animation_length);
	}

	switch (track_type) {
		case Animation::TYPE_POSITION_3D: {
			Vector3 curr_pos = animation->position_track_interpolate(track_index, p_time);
			if (has_looped) {
				Vector3 prev_pos = animation->position_track_interpolate(track_index, end_remainder);
				Vector3 end_pos = animation->position_track_interpolate(track_index, animation_length);
				Vector3 start_pos = animation->position_track_interpolate(track_index, 0.0);
				Vector3 loop_delta(0., 0., 0.);

				if (loop_count > 0) {
					loop_delta = (end_pos - start_pos) * loop_count;
				}

				transform_track_value->loc = end_pos - prev_pos + curr_pos - start_pos + loop_delta;
			} else {
				Vector3 prev_pos = animation->position_track_interpolate(track_index, p_time - delta_time);
				transform_track_value->loc = curr_pos - prev_pos;
			}
			break;
		}
		case Animation::TYPE_ROTATION_3D: {
			Quaternion curr_rot = animation->rotation_track_interpolate(track_index, p_time);
			if (has_looped) {
				Quaternion prev_rot = animation->rotation_track_interpolate(track_index, end_remainder);
				Quaternion end_rot = animation->rotation_track_interpolate(track_index, animation_length);
				Quaternion start_rot = animation->rotation_track_interpolate(track_index, 0.0);
				Quaternion loop_delta(0., 0., 0., 1.);

				if (loop_count > 0) {
					loop_delta = (start_rot.inverse() * end_rot) * loop_count;
				}

				transform_track_value->rot = (prev_rot.inverse() * end_rot * start_rot.inverse() * curr_rot * loop_delta).normalized();
			} else {
				Quaternion prev_rot = animation->rotation_track_interpolate(track_index, p_time - delta_time);
				transform_track_value->rot = (prev_rot.inverse() * curr_rot).normalized();
			}
			break;
		}
		case Animation::TYPE_SCALE_3D: {
			break;
		}
		default: {
			assert(false && !"Invalid root motion track type");
			break;
		}
	}
}

void AnimationData::allocate_track_value(const Ref<Animation> &animation, int32_t track_index, const Skeleton3D *skeleton_3d) {
	Animation::TrackType track_type = animation->track_get_type(track_index);
	AnimationTrackUID track_unique_id = create_track_uid(animation->track_get_path(track_index), track_type);

	switch (track_type) {
		case Animation::TrackType::TYPE_ROTATION_3D:
		case Animation::TrackType::TYPE_POSITION_3D: {
			size_t value_index = 0;
			AnimationData::TransformTrackValue *transform_track_value = nullptr;
			if (track_value_index.has(track_unique_id)) {
				value_index = track_value_index[track_unique_id];
			} else {
				value_index = transform_values.size();
				track_value_index.insert(track_unique_id, value_index);
				transform_values.push_back(AnimationData::TransformTrackValue());
			}
			transform_track_value = &transform_values[value_index];
			assert(transform_track_value != nullptr);

			const NodePath &track_node_path = animation->track_get_path(track_index);
			if (track_node_path.get_subname_count() == 1) {
				transform_track_value->bone_idx = skeleton_3d->find_bone(track_node_path.get_subname(0));
			}

			if (track_type == Animation::TrackType::TYPE_POSITION_3D) {
				transform_track_value->loc_used = true;
			} else if (track_type == Animation::TrackType::TYPE_ROTATION_3D) {
				transform_track_value->rot_used = true;
			}

			break;
		}
		default:
			break;
	}
}

void AnimationData::allocate_track_values(const Ref<Animation> &animation, const Skeleton3D *skeleton_3d) {
	GodotProfileZone("AnimationData::allocate_track_values");

	int count = animation->get_track_count();
	for (int track_index = 0; track_index < count; track_index++) {
		if (!animation->track_is_enabled(track_index)) {
			continue;
		}

		allocate_track_value(animation, track_index, skeleton_3d);
	}
}

void AnimationDataAllocator::register_track_values(const Ref<Animation> &animation, const Skeleton3D *skeleton_3d) {
	default_data.allocate_track_values(animation, skeleton_3d);
}
