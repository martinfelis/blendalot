#pragma once

#ifdef BLENDALOT_GDEXTENSION
#include "gdextension_helper.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/skeleton3d.hpp>

// Hack of a Hack (see comments in Animation::Track::get_unique_id()
typedef uint64_t AnimationTrackUID;

// Disable profiling
#define GodotProfileZone(m_zone_name)
#else
#include "core/profiling/profiling.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/animation_library.h"

// Hack of a Hack (see comments in Animation::Track::get_unique_id()
typedef Animation::TrackCacheID AnimationTrackUID;
#endif

#include "sync_track.h"

#include <cassert>

/**
 * @class AnimationData
 * Represents data that is transported via animation connections in the SyncedAnimationGraph.
 *
 * In general AnimationData objects should be obtained using the AnimationDataAllocator.
 *
 * The class consists of a buffer containing the data and a hashmap that resolves the
 * AnimationTrackUID of an Animation::Track to the corresponding AnimationData::TrackValue
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
	};

	struct TransformTrackValue final : public TrackValue {
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
		track_value_index = other.track_value_index;
		transform_values = other.transform_values;
	}
	AnimationData(AnimationData &&other) noexcept :
			// We skip copying the offset as that should be identical for all nodes within a BLTAnimationGraph.
			// value_buffer_offset(std::exchange(other.value_buffer_offset, AHashMap<AnimationTrackUID, size_t, HashHasher>())),
			transform_values(std::exchange(other.transform_values, LocalVector<TransformTrackValue>())) {
	}
	AnimationData &operator=(const AnimationData &other) {
		AnimationData temp(other);
		std::swap(track_value_index, temp.track_value_index);
		std::swap(transform_values, temp.transform_values);
		return *this;
	}
	AnimationData &operator=(AnimationData &&other) noexcept {
		std::swap(track_value_index, other.track_value_index);
		std::swap(transform_values, other.transform_values);
		return *this;
	}

	void allocate_track_value(const Ref<Animation> &animation, int32_t track_index, const Skeleton3D *skeleton_3d);
	void allocate_track_values(const Ref<Animation> &animation, const Skeleton3D *skeleton_3d);

	template <typename TrackValueType>
	TrackValueType *get_value_at_index(const int32_t &value_index) {
		return reinterpret_cast<TrackValueType *>(&transform_values[value_index]);
	}

	template <typename TrackValueType>
	const TrackValueType *get_value_at_index(const int32_t &value_index) const {
		return reinterpret_cast<const TrackValueType *>(&transform_values[value_index]);
	}

	template <typename TrackValueType>
	TrackValueType *get_value_for_track_uid(const AnimationTrackUID &track_uid) {
		int32_t value_index = get_value_index_from_unique_id(track_uid);
		return get_value_at_index<TrackValueType>(value_index);
	}

	template <typename TrackValueType>
	const TrackValueType *get_value_for_track_uid(const AnimationTrackUID &track_uid) const {
		int32_t value_index = get_value_index_from_unique_id(track_uid);
		return get_value_at_index<TrackValueType>(value_index);
	}

	bool has_same_tracks(const AnimationData &other) const {
		HashSet<AnimationTrackUID> valid_track_hashes;
		for (const KeyValue<AnimationTrackUID, size_t> &K : track_value_index) {
			valid_track_hashes.insert(K.key);
		}

		for (const KeyValue<AnimationTrackUID, size_t> &K : other.track_value_index) {
			if (HashSet<AnimationTrackUID>::Iterator entry = valid_track_hashes.find(K.key)) {
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

		for (unsigned int i = 0; i < transform_values.size(); i++) {
			transform_values[i].blend(to_data.transform_values[i], lambda);
		}
	}

	static Animation::TrackType get_cache_type(Animation::TrackType p_type) {
		if (p_type == Animation::TYPE_BEZIER) {
			return Animation::TYPE_VALUE;
		}
		if (p_type == Animation::TYPE_ROTATION_3D || p_type == Animation::TYPE_SCALE_3D) {
			return Animation::TYPE_POSITION_3D; // Reference them as position3D tracks, even if they modify rotation or scale.
		}
		return p_type;
	}

	static AnimationTrackUID create_track_uid(const NodePath &track_path, const Animation::TrackType &track_type) {
		return track_path.hash() * 10 + get_cache_type(track_type);
	}

	void sample_from_animation(const Ref<Animation> &animation, double p_time, double delta_time = -1, int root_bone_track_index = -1);

	void sample_root_bone(const Ref<Animation> &animation, const int32_t track_index, const Animation::TrackType track_type, const double &p_time, double delta_time, TransformTrackValue *transform_track_value);

	int32_t get_value_index_from_unique_id(AnimationTrackUID track_uid) const {
		if (track_value_index.has(track_uid)) {
			return static_cast<int32_t>(track_value_index[track_uid]);
		}

		return -1;
	}

	AHashMap<AnimationTrackUID, size_t, HashHasher> track_value_index;
	LocalVector<TransformTrackValue> transform_values;
};

template <>
inline AnimationData::TransformTrackValue *AnimationData::get_value_at_index<AnimationData::TransformTrackValue>(const int32_t &value_index) {
	return &transform_values[value_index];
}

template <>
inline const AnimationData::TransformTrackValue *AnimationData::get_value_at_index<AnimationData::TransformTrackValue>(const int32_t &value_index) const {
	return &transform_values[value_index];
}

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
			memcpy(static_cast<void *>(result->transform_values.ptr()), static_cast<void *>(default_data.transform_values.ptr()), default_data.transform_values.size() * sizeof(AnimationData::TransformTrackValue));

			return result;
		}

		AnimationData *result = memnew(AnimationData);
		*result = default_data;
		return result;
	}

	void free(AnimationData *data) {
		allocated_data.push_front(data);
	}

	int get_bone_track_index(const NodePath &root_bone_path) {
		return default_data.get_value_index_from_unique_id(AnimationData::create_track_uid(root_bone_path, Animation::TYPE_POSITION_3D));
	}
};

struct GraphEvaluationContext {
	AnimationPlayer *animation_player = nullptr;
	Skeleton3D *skeleton_3d = nullptr;
	AnimationDataAllocator animation_data_allocator;
	LocalVector<String> validation_messages;
	int32_t root_bone_value_index = -1;
	double graph_process_delta_time = 0.f;
};
