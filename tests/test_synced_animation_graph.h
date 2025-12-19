#pragma once

#include "../synced_animation_graph.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

struct SyncedAnimationGraphFixture {
	Node *character_node;
	Skeleton3D *skeleton_node;
	AnimationPlayer *player_node;

	int hip_bone_index = -1;

	Ref<Animation> test_animation;
	Ref<AnimationLibrary> animation_library;

	SyncedAnimationGraph *synced_animation_graph;
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
		test_animation->track_set_path(track_index, NodePath(vformat("%s:%s", skeleton_node->get_path().get_concatenated_names(), "Hips")));

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
	BlendTreeBuilder tree_constructor;

	Ref<AnimationSamplerNode> animation_sampler_node0;
	animation_sampler_node0.instantiate();
	animation_sampler_node0->name = "Sampler0";
	tree_constructor.add_node(animation_sampler_node0);

	Ref<AnimationSamplerNode> animation_sampler_node1;
	animation_sampler_node1.instantiate();
	animation_sampler_node1->name = "Sampler1";
	tree_constructor.add_node(animation_sampler_node1);

	Ref<AnimationSamplerNode> animation_sampler_node2;
	animation_sampler_node2.instantiate();
	animation_sampler_node2->name = "Sampler2";
	tree_constructor.add_node(animation_sampler_node2);

	Ref<AnimationBlend2Node> node_blend0;
	node_blend0.instantiate();
	node_blend0->name = "Blend0";
	tree_constructor.add_node(node_blend0);

	Ref<AnimationBlend2Node> node_blend1;
	node_blend1.instantiate();
	node_blend1->name = "Blend1";
	tree_constructor.add_node(node_blend1);

	// Tree
	// Sampler0 -\
	// Sampler1 -+ Blend0 -\
	// Sampler2 -----------+ Blend1 - Output

	CHECK(tree_constructor.add_connection(animation_sampler_node0, node_blend0, "Input0"));

	// Ensure that subtree is properly updated
	int sampler0_index = tree_constructor.get_node_index(animation_sampler_node0);
	int blend0_index = tree_constructor.get_node_index(node_blend0);
	CHECK(tree_constructor.node_connection_info[blend0_index].input_subtree_node_indices.has(sampler0_index));

	// Connect blend0 to blend1
	CHECK(tree_constructor.add_connection(node_blend0, node_blend1, "Input0"));

	// Connecting to an already connected port must fail
	CHECK(!tree_constructor.add_connection(animation_sampler_node1, node_blend0, "Input0"));
	// Correct connection of Sampler1 to Blend0
	CHECK(tree_constructor.add_connection(animation_sampler_node1, node_blend0, "Input1"));

	// Ensure that subtree is properly updated
	int sampler1_index = tree_constructor.get_node_index(animation_sampler_node0);
	int blend1_index = tree_constructor.get_node_index(node_blend1);
	CHECK(tree_constructor.node_connection_info[blend1_index].input_subtree_node_indices.has(sampler1_index));
	CHECK(tree_constructor.node_connection_info[blend1_index].input_subtree_node_indices.has(sampler0_index));
	CHECK(tree_constructor.node_connection_info[blend1_index].input_subtree_node_indices.has(blend0_index));

	// Creating a loop must fail
	CHECK(!tree_constructor.add_connection(node_blend1, node_blend0, "Input1"));

	// Perform remaining connections
	CHECK(tree_constructor.add_connection(node_blend1, tree_constructor.get_output_node(), "Input"));
	CHECK(tree_constructor.add_connection(animation_sampler_node2, node_blend1, "Input1"));

	// Output node must have all nodes in its subtree:
	CHECK(tree_constructor.node_connection_info[0].input_subtree_node_indices.has(1));
	CHECK(tree_constructor.node_connection_info[0].input_subtree_node_indices.has(2));
	CHECK(tree_constructor.node_connection_info[0].input_subtree_node_indices.has(3));
	CHECK(tree_constructor.node_connection_info[0].input_subtree_node_indices.has(4));
	CHECK(tree_constructor.node_connection_info[0].input_subtree_node_indices.has(5));

	tree_constructor.sort_nodes_and_references();

	// Check that for node i all input nodes have a node index j > i.
	for (unsigned int i = 0; i < tree_constructor.nodes.size(); i++) {
		for (int input_index : tree_constructor.node_connection_info[i].input_subtree_node_indices) {
			CHECK(input_index > i);
		}
	}
}

TEST_CASE_FIXTURE(SyncedAnimationGraphFixture, "[SceneTree][SyncedAnimationGraph] SyncedAnimationGraph with an AnimationSampler as root node") {
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

TEST_CASE_FIXTURE(SyncedAnimationGraphFixture, "[SceneTree][SyncedAnimationGraph][BlendTree] BlendTree with a AnimationSamplerNode connected to the output") {
	Ref<SyncedBlendTree> synced_blend_tree_node;
	synced_blend_tree_node.instantiate();

	Ref<AnimationSamplerNode> animation_sampler_node;
	animation_sampler_node.instantiate();
	animation_sampler_node->animation_name = "animation_library/TestAnimation";

	synced_blend_tree_node->add_node(animation_sampler_node);
	REQUIRE(synced_blend_tree_node->add_connection(animation_sampler_node, synced_blend_tree_node->get_output_node(), "Input"));

	synced_blend_tree_node->initialize(synced_animation_graph->get_context());

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

} //namespace TestSyncedAnimationGraph