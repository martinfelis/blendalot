#pragma once

#include "../synced_animation_graph.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_server_default.h"

#include "tests/test_macros.h"

namespace TestSyncedAnimationGraph {
TEST_CASE("[SceneTree][SyncedAnimationGraph] Simple") {
	Node* character_node = memnew(Node);
	character_node->set_name("CharacterNode");
	SceneTree::get_singleton()->get_root()->add_child(character_node);

	Skeleton3D *skeleton_node = memnew(Skeleton3D);
	skeleton_node->set_name("Skeleton");
	character_node->add_child(skeleton_node);

	skeleton_node->add_bone("Root");
	int hip_bone_index = skeleton_node->add_bone("Hips");

	AnimationPlayer *player_node = memnew(AnimationPlayer);
	player_node->set_name("AnimationPlayer");

	Ref<Animation> animation = memnew(Animation);
	const int track_index = animation->add_track(Animation::TYPE_POSITION_3D);
	CHECK(track_index == 0);
	animation->track_insert_key(track_index, 0.0, Vector3(0., 0., 0.));
	animation->track_insert_key(track_index, 1.0, Vector3(1., 2., 3.));
	animation->track_set_path(track_index, NodePath(vformat("%s:%s", skeleton_node->get_path().get_concatenated_names(),"Hips")));

	Ref<AnimationLibrary> animation_library;
	animation_library.instantiate();
	animation_library->add_animation("TestAnimation", animation);

	player_node->add_animation_library("animation_library", animation_library);
	SceneTree::get_singleton()->get_root()->add_child(player_node);

	SyncedAnimationGraph *synced_animation_graph = memnew(SyncedAnimationGraph);
	SceneTree::get_singleton()->get_root()->add_child(synced_animation_graph);

	synced_animation_graph->set_animation_player(player_node->get_path());
	synced_animation_graph->set_skeleton(skeleton_node->get_path());

	Ref<AnimationSamplerNode> animation_sampler_node;
	animation_sampler_node.instantiate();
	animation_sampler_node->animation_name = "animation_library/TestAnimation";

	synced_animation_graph->set_graph_root_node(animation_sampler_node);

	Vector3 hip_bone_position = skeleton_node->get_bone_global_pose(hip_bone_index).origin;

	CHECK(hip_bone_position.x == doctest::Approx(0.0));
	CHECK(hip_bone_position.y == doctest::Approx(0.0));
	CHECK(hip_bone_position.z == doctest::Approx(0.0));

	SceneTree::get_singleton()->process(0.01);

	hip_bone_position = skeleton_node->get_bone_global_pose(hip_bone_index).origin;

	CHECK(hip_bone_position.x == doctest::Approx(0.01));
	CHECK(hip_bone_position.y == doctest::Approx(0.02));
	CHECK(hip_bone_position.z == doctest::Approx(0.03));
}
}
