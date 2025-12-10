#pragma once

#include "../synced_animation_graph.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_server_default.h"

#include "tests/test_macros.h"

struct SyncedAnimationGraphFixture {
	Node* character_node;
	Skeleton3D* skeleton_node;
	AnimationPlayer* player_node;

	int hip_bone_index = -1;

	Ref<Animation> test_animation;
	Ref<AnimationLibrary> animation_library;

	SyncedAnimationGraph* synced_animation_graph;
	SyncedAnimationGraphFixture() {
		character_node = memnew(Node);
		character_node->set_name("CharacterNode");
		SceneTree::get_singleton()->get_root()->add_child(character_node);

		skeleton_node = memnew(Skeleton3D);
		skeleton_node->set_name("Skeleton");
		character_node->add_child(skeleton_node);

		skeleton_node->add_bone("Root");
		hip_bone_index = skeleton_node->add_bone("Hips");

		player_node = memnew(AnimationPlayer);
		player_node->set_name("AnimationPlayer");

		test_animation = memnew(Animation);
		const int track_index = test_animation->add_track(Animation::TYPE_POSITION_3D);
		CHECK(track_index == 0);
		test_animation->track_insert_key(track_index, 0.0, Vector3(0., 0., 0.));
		test_animation->track_insert_key(track_index, 1.0, Vector3(1., 2., 3.));
		test_animation->track_set_path(track_index, NodePath(vformat("%s:%s", skeleton_node->get_path().get_concatenated_names(),"Hips")));

		animation_library.instantiate();
		animation_library->add_animation("TestAnimation", test_animation);

		player_node->add_animation_library("animation_library", animation_library);
		SceneTree::get_singleton()->get_root()->add_child(player_node);

		synced_animation_graph = memnew(SyncedAnimationGraph);
		SceneTree::get_singleton()->get_root()->add_child(synced_animation_graph);

		synced_animation_graph->set_animation_player(player_node->get_path());
		synced_animation_graph->set_skeleton(skeleton_node->get_path());
	}
};

namespace TestSyncedAnimationGraph {

TEST_CASE("[SyncedAnimationGraph] TestBlendTreeConstruction") {
	SortedTreeConstructor tree_constructor;

	Ref<AnimationSamplerNode> animation_sampler_node0;
	animation_sampler_node0.instantiate();
	animation_sampler_node0->name = "Sampler0";
	tree_constructor.add_node(animation_sampler_node0);

	Ref<AnimationSamplerNode> animation_sampler_node1;
	animation_sampler_node1.instantiate();
	animation_sampler_node1->name = "Sampler1";
	tree_constructor.add_node(animation_sampler_node1);

	Ref<AnimationBlend2Node> node_blend0;
	node_blend0.instantiate();
	node_blend0->name = "Blend2";
	tree_constructor.add_node(node_blend0);

	Ref<AnimationBlend2Node> node_blend1;
	node_blend1.instantiate();
	node_blend1->name = "Blend2";
	tree_constructor.add_node(node_blend1);

	CHECK(tree_constructor.add_connection(animation_sampler_node0, node_blend0, "Input0"));
	CHECK(tree_constructor.add_connection(node_blend1, node_blend0, "Input1"));
	CHECK(!tree_constructor.add_connection(node_blend1, node_blend0, "Input1"));
}

TEST_CASE_FIXTURE(SyncedAnimationGraphFixture, "[SceneTree][SyncedAnimationGraph] SimpleAnimationSamplerTest" * doctest::skip(true)) {
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

TEST_CASE_FIXTURE(SyncedAnimationGraphFixture, "[SceneTree][SyncedAnimationGraph] SimpleBlendTreeTest" * doctest::skip(true)) {
	Ref<SyncedBlendTree> synced_blend_tree_node;
	synced_blend_tree_node.instantiate();

	Ref<AnimationSamplerNode> animation_sampler_node;
	animation_sampler_node.instantiate();
	animation_sampler_node->animation_name = "animation_library/TestAnimation";
	synced_blend_tree_node->add_node(animation_sampler_node);


	synced_blend_tree_node->connect_nodes(animation_sampler_node, synced_blend_tree_node->get_output_node(), "Input");

	synced_animation_graph->set_graph_root_node(synced_blend_tree_node);

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