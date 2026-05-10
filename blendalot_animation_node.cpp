//
// Created by martin on 03.12.25.
//

#include "blendalot_animation_node.h"

#ifdef BLENDALOT_MODULE
#include "core/object/class_db.h"
#include "scene/resources/animation.h"
#endif

#ifdef BLENDALOT_GDEXTENSION
#include "gdextension_helper.h"
#endif

void BLTAnimationNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_position", "position"), &BLTAnimationNode::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &BLTAnimationNode::get_position);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_position", "get_position");
	ADD_SIGNAL(MethodInfo("animation_node_renamed", PropertyInfo(Variant::INT, "object_id"), PropertyInfo(Variant::STRING, "old_name"), PropertyInfo(Variant::STRING, "new_name")));
	ADD_SIGNAL(MethodInfo("animation_node_removed", PropertyInfo(Variant::INT, "object_id"), PropertyInfo(Variant::STRING, "name")));
	ADD_SIGNAL(MethodInfo(SNAME("node_changed"), PropertyInfo(Variant::STRING_NAME, "node_name")));
	ClassDB::bind_method(D_METHOD("get_input_names"), &BLTAnimationNode::get_input_names_as_typed_array);
	ClassDB::bind_method(D_METHOD("get_input_count"), &BLTAnimationNode::get_input_count);
	ClassDB::bind_method(D_METHOD("get_input_index", "node"), &BLTAnimationNode::get_input_index);
}

void BLTAnimationNode::get_parameter_list(List<PropertyInfo> *r_list) const {
}

Variant BLTAnimationNode::get_parameter_default_value(const StringName &p_parameter) const {
	return Variant();
}

bool BLTAnimationNode::is_parameter_read_only(const StringName &p_parameter) const {
	return false;
}

void BLTAnimationNode::set_parameter(const StringName &p_name, const Variant &p_value) {
}

Variant BLTAnimationNode::get_parameter(const StringName &p_name) const {
	return Variant();
}

void BLTAnimationNode::_node_changed() {
	emit_signal(SNAME("node_changed"), get_name());
}

void BLTAnimationNode::_animation_node_renamed(const ObjectID &p_oid, const String &p_old_name, const String &p_new_name) {
	emit_signal(SNAME("animation_node_renamed"), p_oid, p_old_name, p_new_name);
}

void BLTAnimationNode::_animation_node_removed(const ObjectID &p_oid, const StringName &p_node) {
	emit_signal(SNAME("animation_node_removed"), p_oid, p_node);
}

void AnimationData::sample_from_animation(const Ref<Animation> &animation, double p_time, const double delta_time, const int root_bone_track_index) {
	GodotProfileZone("AnimationData::sample_from_animation");

	int count = animation->get_track_count();
	for (int32_t track_index = 0; track_index < count; track_index++) {
		if (!animation->track_is_enabled(track_index)) {
			continue;
		}

		Animation::TrackType track_type = animation->track_get_type(track_index);
		AnimationTrackUID track_uid = create_track_uid(animation->track_get_path(track_index), track_type);
		switch (track_type) {
			case Animation::TYPE_POSITION_3D:
			case Animation::TYPE_ROTATION_3D: {
				TransformTrackValue *transform_track_value = get_value<TransformTrackValue>(track_uid);

				if (transform_track_value->bone_idx != -1) {
					if (track_index == root_bone_track_index) {
						sample_root_bone(animation, track_index, track_type, root_bone_track_index, p_time, delta_time, transform_track_value);
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

void AnimationData::sample_root_bone(const Ref<Animation> &animation, const int32_t track_index, const Animation::TrackType track_type, const int root_bone_index, const double &p_time, double delta_time, AnimationData::TransformTrackValue *transform_track_value) {
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

//
// BLTAnimationNodeSampler
//
bool BLTAnimationNodeSampler::initialize(GraphEvaluationContext &context) {
	if (!BLTAnimationNode::initialize(context)) {
		return false;
	}

	animation_player = context.animation_player;

	if (animation_player == nullptr) {
		context.validation_messages.push_back(vformat("Invalid node %s: No animation player specified for node.", node_path));
		return false;
	}

	if (animation_name.is_empty()) {
		context.validation_messages.push_back(vformat("Invalid node %s: No animation specified for playback.", node_path));
		return false;
	}

	if (!set_animation(animation_name)) {
		context.validation_messages.push_back(vformat("Invalid node %s: Could not set animation %s.", node_path, animation_name));
		return false;
	}

	context.animation_data_allocator.register_track_values(animation, context.skeleton_3d);

	return true;
}

void BLTAnimationNodeSampler::update_time(const double p_delta, const double p_time) {
	BLTAnimationNode::update_time(p_delta, p_time);

	if (node_time_info.is_synced) {
		// Convert the sync time to actual animation time.
		node_time_info.position = node_time_info.sync_track.calc_ratio_from_sync_time(node_time_info.sync_position) * animation->get_length();
		// Convert the delta into the corresponding animation time delta.
		return;
	}

	if (node_time_info.loop_mode != Animation::LOOP_NONE) {
		if (node_time_info.loop_mode == Animation::LOOP_LINEAR) {
			if (!Math::is_zero_approx(animation->get_length())) {
				node_time_info.position = Math::fposmod(node_time_info.position, static_cast<double>(animation->get_length()));
			}
		} else {
			assert(false && !"Ping-pong looping not yet supported");
		}
	}
}

void BLTAnimationNodeSampler::evaluate(GraphEvaluationContext &context, const LocalVector<AnimationData *> &inputs, AnimationData &output) {
	GodotProfileZone("AnimationSamplerNode::evaluate");

	assert(inputs.size() == 0);

	output.sample_from_animation(animation, node_time_info.position, node_time_info.delta, context.root_bone_track_index);
}

void BLTAnimationNodeSampler::set_animation_player(AnimationPlayer *p_player) {
	animation_player = p_player;
	_node_changed();
}

bool BLTAnimationNodeSampler::set_animation(const String &p_name) {
	bool has_animation_name_changed = p_name != animation_name;
	animation_name = p_name;

	if (animation_player == nullptr) {
		return false;
	}

	if (!animation_player->has_animation(p_name)) {
		if (has_animation_name_changed) {
			_node_changed();
		}
		return false;
	}

	animation = animation_player->get_animation(p_name);
	if (!animation.is_valid()) {
		print_error(vformat("Cannot initialize node %s: animation '%s' not found in animation player.", get_name(), animation_name));

		_node_changed();
		return false;
	}

	node_time_info.loop_mode = animation->get_loop_mode();

	// Initialize Sync Track from marker
	LocalVector<float> sync_markers;
	int marker_index = 0;
	String marker_name = itos(marker_index);
	while (animation->has_marker(marker_name)) {
		sync_markers.push_back(animation->get_marker_time(marker_name));
		marker_index++;
		marker_name = itos(marker_index);
	}

	if (sync_markers.size() > 0) {
		node_time_info.sync_track = SyncTrack::create_from_markers(animation->get_length(), sync_markers);
	} else {
		node_time_info.sync_track = SyncTrack::create_from_markers(animation->get_length(), { 0 });
	}

	if (has_animation_name_changed) {
		_node_changed();
	}

	return true;
}

StringName BLTAnimationNodeSampler::get_animation() const {
	return animation_name;
}

AnimationPlayer *BLTAnimationNodeSampler::get_animation_player() const {
	return animation_player;
}

TypedArray<StringName> BLTAnimationNodeSampler::get_animations_as_typed_array() const {
	TypedArray<StringName> typed_arr;

	if (animation_player == nullptr) {
		print_error(vformat("BLTAnimationNodeSampler '%s' not yet initialized", get_name()));
		return typed_arr;
	}

#ifdef BLENDALOT_MODULE
	Vector<StringName> vec;

	LocalVector<StringName> animation_libraries;
	animation_player->get_animation_library_list(&animation_libraries);

	for (const StringName &library_name : animation_libraries) {
		Ref<AnimationLibrary> library = animation_player->get_animation_library(library_name);
		LocalVector<StringName> library_animations;
		library->get_animation_list(&library_animations);
		for (const StringName &library_animation : library_animations) {
			vec.push_back(library_animation);
		}
	}

	typed_arr.resize(vec.size());
	for (uint32_t i = 0; i < vec.size(); i++) {
		typed_arr[i] = vec[i];
	}
	return typed_arr;
#endif

#ifdef BLENDALOT_GDEXTENSION
	return animation_player->get_animation_library_list();
#endif
}

void BLTAnimationNodeSampler::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_animation", "name"), &BLTAnimationNodeSampler::set_animation);
	ClassDB::bind_method(D_METHOD("get_animation"), &BLTAnimationNodeSampler::get_animation);
	ClassDB::bind_method(D_METHOD("get_animation_player"), &BLTAnimationNodeSampler::get_animation_player);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "animation"), "set_animation", "get_animation");

	ClassDB::bind_method(D_METHOD("get_animations"), &BLTAnimationNodeSampler::get_animations_as_typed_array);
}

//
// BLTAnimationNodeTimeScale
//
void BLTAnimationNodeTimeScale::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::FLOAT, scale_name, PROPERTY_HINT_RANGE, "-10,10,0.01,or_less,or_greater"));
}

bool BLTAnimationNodeTimeScale::_get(const StringName &p_name, Variant &r_value) const {
	if (p_name == scale_name) {
		r_value = scale;
		return true;
	}

	return false;
}

bool BLTAnimationNodeTimeScale::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == scale_name) {
		scale = p_value;
		return true;
	}

	return false;
}

//
// BLTAnimationNodeBlend2
//
void BLTAnimationNodeBlend2::evaluate(GraphEvaluationContext &context, const LocalVector<AnimationData *> &inputs, AnimationData &output) {
	GodotProfileZone("AnimationBlend2Node::evaluate");

	if (Math::is_zero_approx(blend_weight)) {
		output = std::move(*inputs[0]);
		return;
	}

	if (Math::is_zero_approx(1. - blend_weight)) {
		output = std::move(*inputs[1]);
		return;
	}

	output = std::move(*inputs[0]);
	output.blend(*inputs[1], blend_weight);
}

void BLTAnimationNodeBlend2::set_use_sync(bool p_sync) {
	sync = p_sync;
}

bool BLTAnimationNodeBlend2::is_using_sync() const {
	return sync;
}

void BLTAnimationNodeBlend2::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_use_sync", "enable"), &BLTAnimationNodeBlend2::set_use_sync);
	ClassDB::bind_method(D_METHOD("is_using_sync"), &BLTAnimationNodeBlend2::is_using_sync);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sync"), "set_use_sync", "is_using_sync");
}

void BLTAnimationNodeBlend2::get_parameter_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::FLOAT, blend_weight_pname, PROPERTY_HINT_RANGE, "0,1,0.01,or_less,or_greater"));
}

void BLTAnimationNodeBlend2::set_parameter(const StringName &p_name, const Variant &p_value) {
	_set(p_name, p_value);
}

Variant BLTAnimationNodeBlend2::get_parameter(const StringName &p_name) const {
	Variant result;
	_get(p_name, result);
	return result;
}

Variant BLTAnimationNodeBlend2::get_parameter_default_value(const StringName &p_parameter) const {
	if (p_parameter == blend_weight_pname) {
		return blend_weight;
	}

	return Variant();
}

void BLTAnimationNodeBlend2::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::FLOAT, blend_weight_pname, PROPERTY_HINT_RANGE, "0,1,0.01,or_less,or_greater"));
}

bool BLTAnimationNodeBlend2::_get(const StringName &p_name, Variant &r_value) const {
	if (p_name == blend_weight_pname) {
		r_value = blend_weight;
		return true;
	}

	return false;
}

bool BLTAnimationNodeBlend2::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == blend_weight_pname) {
		blend_weight = CLAMP<float>(p_value, 0.0f, 1.0f);
		return true;
	}

	return false;
}