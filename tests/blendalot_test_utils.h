#pragma once

#include "../blendalot_animation_node.h"

template <typename T>
inline bool BLTEqualApproxReport(const T &reference, const T &value, double eps = 1.0e-5) {
	return false;
}

template <>
inline bool BLTEqualApproxReport<Vector3>(const Vector3 &reference, const Vector3 &value, double eps) {
	bool is_approx_equal = reference.x == doctest::Approx(value.x) //
			&& reference.y == doctest::Approx(value.y) //
			&& reference.z == doctest::Approx(value.z);

	if (!is_approx_equal) {
		print_line(vformat("Vector3 mismatch:\n  (%f, %f, %f) != (%f, %f, %f)",
				reference.x, reference.y, reference.z, value.x, value.y, value.z));
	}

	return is_approx_equal;
}

template <>
inline bool BLTEqualApproxReport<Quaternion>(const Quaternion &reference, const Quaternion &value, double eps) {
	bool is_approx_equal = reference.x == doctest::Approx(value.x) //
			&& reference.y == doctest::Approx(value.y) //
			&& reference.z == doctest::Approx(value.z) //
			&& reference.w == doctest::Approx(value.w);

	if (!is_approx_equal) {
		print_line(vformat("Quaternion mismatch:\n  (%f, %f, %f, %f) != (%f, %f, %f, %f)",
				reference.x, reference.y, reference.z, reference.w, value.x, value.y, value.z, value.w));
	}

	return is_approx_equal;
}

template <>
inline bool BLTEqualApproxReport<AnimationData::TrackValue>(const AnimationData::TrackValue &reference, const AnimationData::TrackValue &value, double eps) {
	if (reference.type != value.type) {
		print_line("Invalid AnimationData::TrackValue comparison.");
		return false;
	}

	if (reference.type != AnimationData::TYPE_POSITION_3D) {
		print_line("AnimationData::TrackValue comparison not yet implemented.");
		return false;
	}

	const AnimationData::TransformTrackValue &reference_transform = reinterpret_cast<const AnimationData::TransformTrackValue &>(reference);
	const AnimationData::TransformTrackValue &value_transform = reinterpret_cast<const AnimationData::TransformTrackValue &>(value);

	bool is_approx_equal = reference_transform.loc.x == doctest::Approx(value_transform.loc.x) //
			&& reference_transform.loc.y == doctest::Approx(value_transform.loc.y) //
			&& reference_transform.loc.z == doctest::Approx(value_transform.loc.z) //
			&& reference_transform.rot.x == doctest::Approx(value_transform.rot.x) //
			&& reference_transform.rot.y == doctest::Approx(value_transform.rot.y) //
			&& reference_transform.rot.z == doctest::Approx(value_transform.rot.z) //
			&& reference_transform.rot.w == doctest::Approx(value_transform.rot.w);

	if (!is_approx_equal) {
		print_line(vformat("Transform mismatch:\n  loc (%f, %f, %f) != (%f, %f, %f)\n  rot (%f, %f, %f, %f) != (%f, %f, %f, %f)",
				reference_transform.loc.x, reference_transform.loc.y, reference_transform.loc.z, value_transform.loc.x, value_transform.loc.y, value_transform.loc.z, //
				reference_transform.rot.x, reference_transform.rot.y, reference_transform.rot.z, reference_transform.rot.w, value_transform.rot.x, value_transform.rot.y, value_transform.rot.z, value_transform.rot.z));
	}

	return is_approx_equal;
}
