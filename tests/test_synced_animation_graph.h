#pragma once

#include "../blendalot_animation_graph.h"
#include "scene/animation/animation_tree.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

struct SyncedAnimationGraphFixture {
	Node *character_node;
	Skeleton3D *skeleton_node;
	AnimationPlayer *player_node;

	int hip_bone_index = -1;

	Ref<Animation> test_animation_a;
	Ref<Animation> test_animation_b;
	Ref<Animation> test_animation_sync_a;
	Ref<Animation> test_animation_sync_b;

	Ref<AnimationLibrary> animation_library;

	BLTAnimationGraph *synced_animation_graph;
	SyncedAnimationGraphFixture() {
		BLTAnimationGraph *scene_animation_graph = dynamic_cast<BLTAnimationGraph *>(SceneTree::get_singleton()->get_root()->find_child("SyncedAnimationGraphFixtureTestNode", true, false));

		if (scene_animation_graph == nullptr) {
			setup_test_scene();
		}

		assign_scene_variables();
	}

	void setup_test_scene() {
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

		setup_animations();

		SceneTree::get_singleton()->get_root()->add_child(player_node);

		synced_animation_graph = memnew(BLTAnimationGraph);
		synced_animation_graph->set_name("SyncedAnimationGraphFixtureTestNode");
		SceneTree::get_singleton()->get_root()->add_child(synced_animation_graph);

		synced_animation_graph->set_animation_player(player_node->get_path());
		synced_animation_graph->set_skeleton(skeleton_node->get_path());
	}

	void setup_animations() {
		test_animation_a = memnew(Animation);
		int track_index = test_animation_a->add_track(Animation::TYPE_POSITION_3D);
		CHECK(track_index == 0);
		test_animation_a->track_insert_key(track_index, 0.0, Vector3(0., 0., 0.));
		test_animation_a->track_insert_key(track_index, 1.0, Vector3(1., 2., 3.));
		test_animation_a->track_set_path(track_index, NodePath(vformat("%s:%s", skeleton_node->get_path().get_concatenated_names(), "Hips")));
		test_animation_a->set_loop_mode(Animation::LOOP_LINEAR);

		animation_library.instantiate();
		animation_library->add_animation("TestAnimationA", test_animation_a);

		test_animation_b = memnew(Animation);
		track_index = test_animation_b->add_track(Animation::TYPE_POSITION_3D);
		CHECK(track_index == 0);
		test_animation_b->track_insert_key(track_index, 0.0, Vector3(0., 0., 0.));
		test_animation_b->track_insert_key(track_index, 1.0, Vector3(2., 4., 6.));
		test_animation_b->track_set_path(track_index, NodePath(vformat("%s:%s", skeleton_node->get_path().get_concatenated_names(), "Hips")));
		test_animation_b->set_loop_mode(Animation::LOOP_LINEAR);

		animation_library->add_animation("TestAnimationB", test_animation_b);

		test_animation_sync_a = memnew(Animation);
		track_index = test_animation_sync_a->add_track(Animation::TYPE_POSITION_3D);
		CHECK(track_index == 0);
		test_animation_sync_a->track_insert_key(track_index, 0.0, Vector3(0., 0., 0.));
		test_animation_sync_a->track_insert_key(track_index, 0.4, Vector3(1., 2., 3.));
		test_animation_sync_a->set_length(2.0);
		test_animation_sync_a->track_set_path(track_index, NodePath(vformat("%s:%s", skeleton_node->get_path().get_concatenated_names(), "Hips")));
		test_animation_sync_a->add_marker("0", 0.0);
		test_animation_sync_a->add_marker("1", 0.4);
		test_animation_sync_a->track_set_interpolation_type(track_index, Animation::INTERPOLATION_LINEAR);
		test_animation_sync_a->set_loop_mode(Animation::LOOP_LINEAR);

		animation_library->add_animation("TestAnimationSyncA", test_animation_sync_a);

		test_animation_sync_b = memnew(Animation);
		track_index = test_animation_sync_b->add_track(Animation::TYPE_POSITION_3D);
		CHECK(track_index == 0);
		test_animation_sync_b->track_insert_key(track_index, 0.1, Vector3(2., 4., 6.));
		test_animation_sync_b->track_insert_key(track_index, 0.2, Vector3(0., 0., 0.));
		test_animation_sync_b->set_length(1.0);
		test_animation_sync_b->track_set_path(track_index, NodePath(vformat("%s:%s", skeleton_node->get_path().get_concatenated_names(), "Hips")));
		test_animation_sync_b->add_marker("1", 0.1);
		test_animation_sync_b->add_marker("0", 0.2);
		test_animation_sync_b->track_set_interpolation_type(track_index, Animation::INTERPOLATION_LINEAR);
		test_animation_sync_b->set_loop_mode(Animation::LOOP_LINEAR);

		animation_library->add_animation("TestAnimationSyncB", test_animation_sync_b);

		player_node->add_animation_library("animation_library", animation_library);
	}

	void assign_scene_variables() {
		synced_animation_graph = dynamic_cast<BLTAnimationGraph *>(SceneTree::get_singleton()->get_root()->find_child("SyncedAnimationGraphFixtureTestNode", true, false));
		REQUIRE(synced_animation_graph);
		character_node = (SceneTree::get_singleton()->get_root()->find_child("CharacterNode", true, false));
		REQUIRE(character_node != nullptr);
		skeleton_node = dynamic_cast<Skeleton3D *>((SceneTree::get_singleton()->get_root()->find_child("Skeleton", true, false)));
		REQUIRE(skeleton_node != nullptr);
		player_node = dynamic_cast<AnimationPlayer *>((SceneTree::get_singleton()->get_root()->find_child("AnimationPlayer", true, false)));
		REQUIRE(player_node != nullptr);

		skeleton_node->reset_bone_poses();
		hip_bone_index = skeleton_node->find_bone("Hips");
		REQUIRE(hip_bone_index > -1);

		animation_library = player_node->get_animation_library("animation_library");
		REQUIRE(animation_library.is_valid());

		test_animation_a = animation_library->get_animation("TestAnimationA");
		REQUIRE(test_animation_a.is_valid());
		test_animation_b = animation_library->get_animation("TestAnimationB");
		REQUIRE(test_animation_b.is_valid());
		test_animation_sync_a = animation_library->get_animation("TestAnimationSyncA");
		REQUIRE(test_animation_sync_a.is_valid());
		test_animation_sync_b = animation_library->get_animation("TestAnimationSyncB");
		REQUIRE(test_animation_sync_b.is_valid());
	}
};

namespace TestSyncedAnimationGraph {

TEST_CASE("[SyncedAnimationGraph] Test BlendTree construction") {
	BlendTreeGraph tree_constructor;

	Ref<BLTAnimationNodeSampler> animation_sampler_node0;
	animation_sampler_node0.instantiate();
	animation_sampler_node0->name = "Sampler0";
	tree_constructor.add_node(animation_sampler_node0);

	Ref<BLTAnimationNodeSampler> animation_sampler_node1;
	animation_sampler_node1.instantiate();
	animation_sampler_node1->name = "Sampler1";
	tree_constructor.add_node(animation_sampler_node1);

	Ref<BLTAnimationNodeSampler> animation_sampler_node2;
	animation_sampler_node2.instantiate();
	animation_sampler_node2->name = "Sampler2";
	tree_constructor.add_node(animation_sampler_node2);

	Ref<BLTAnimationNodeBlend2> node_blend0;
	node_blend0.instantiate();
	node_blend0->name = "Blend0";
	tree_constructor.add_node(node_blend0);

	Ref<BLTAnimationNodeBlend2> node_blend1;
	node_blend1.instantiate();
	node_blend1->name = "Blend1";
	tree_constructor.add_node(node_blend1);

	// Tree
	// Sampler0 -\
	// Sampler1 -+ Blend0 -\
	// Sampler2 -----------+ Blend1 - Output

	CHECK(tree_constructor.add_connection(animation_sampler_node0, node_blend0, "Input0"));

	// Ensure that subtree is properly updated
	int sampler0_index = tree_constructor.find_node_index(animation_sampler_node0);
	int blend0_index = tree_constructor.find_node_index(node_blend0);
	CHECK(tree_constructor.node_connection_info[blend0_index].input_subtree_node_indices.has(sampler0_index));

	// Connect blend0 to blend1
	CHECK(tree_constructor.add_connection(node_blend0, node_blend1, "Input0"));

	// Connecting to an already connected port must fail
	CHECK(!tree_constructor.add_connection(animation_sampler_node1, node_blend0, "Input0"));
	// Correct connection of Sampler1 to Blend0
	CHECK(tree_constructor.add_connection(animation_sampler_node1, node_blend0, "Input1"));

	// Ensure that subtree is properly updated
	int sampler1_index = tree_constructor.find_node_index(animation_sampler_node0);
	int blend1_index = tree_constructor.find_node_index(node_blend1);
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

TEST_CASE_FIXTURE(SyncedAnimationGraphFixture, "[SceneTree][SyncedAnimationGraph] Test AnimationData blending") {
	AnimationData data_t0;
	data_t0.allocate_track_values(test_animation_a, skeleton_node);
	data_t0.sample_from_animation(test_animation_a, skeleton_node, 0.0);

	AnimationData data_t1;
	data_t1.allocate_track_values(test_animation_a, skeleton_node);
	data_t1.sample_from_animation(test_animation_a, skeleton_node, 1.0);

	AnimationData data_t0_5;
	data_t0_5.allocate_track_values(test_animation_a, skeleton_node);
	data_t0_5.sample_from_animation(test_animation_a, skeleton_node, 0.5);

	AnimationData data_blended = data_t0;
	data_blended.blend(data_t1, 0.5);

	REQUIRE(data_blended.has_same_tracks(data_t0_5));
	for (const KeyValue<Animation::TypeHash, size_t> &K : data_blended.value_buffer_offset) {
		AnimationData::TrackValue *blended_value = data_blended.get_value<AnimationData::TrackValue>(K.key);
		AnimationData::TrackValue *data_t0_5_value = data_t0_5.get_value<AnimationData::TrackValue>(K.key);
		CHECK(*blended_value == *data_t0_5_value);
	}

	// And also check that values do not match
	data_blended = data_t0;
	data_blended.blend(data_t1, 0.3);

	REQUIRE(data_blended.has_same_tracks(data_t0_5));
	for (const KeyValue<Animation::TypeHash, size_t> &K : data_blended.value_buffer_offset) {
		AnimationData::TrackValue *blended_value = data_blended.get_value<AnimationData::TrackValue>(K.key);
		AnimationData::TrackValue *data_t0_5_value = data_t0_5.get_value<AnimationData::TrackValue>(K.key);
		CHECK(*blended_value != *data_t0_5_value);
	}
}

TEST_CASE_FIXTURE(SyncedAnimationGraphFixture, "[SceneTree][SyncedAnimationGraph] SyncedAnimationGraph evaluation with an AnimationSampler as root node") {
	Ref<BLTAnimationNodeSampler> animation_sampler_node;
	animation_sampler_node.instantiate();
	animation_sampler_node->animation_name = "animation_library/TestAnimationA";

	synced_animation_graph->set_root_animation_node(animation_sampler_node);

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

TEST_CASE_FIXTURE(SyncedAnimationGraphFixture, "[SceneTree][SyncedAnimationGraph][BlendTree] BlendTree evaluation with a AnimationSamplerNode connected to the output") {
	Ref<BLTAnimationNodeBlendTree> synced_blend_tree_node;
	synced_blend_tree_node.instantiate();

	Ref<BLTAnimationNodeSampler> animation_sampler_node;
	animation_sampler_node.instantiate();
	animation_sampler_node->animation_name = "animation_library/TestAnimationA";

	synced_blend_tree_node->add_node(animation_sampler_node);
	REQUIRE(synced_blend_tree_node->add_connection(animation_sampler_node, synced_blend_tree_node->get_output_node(), "Input"));

	synced_blend_tree_node->initialize(synced_animation_graph->get_context());

	synced_animation_graph->set_root_animation_node(synced_blend_tree_node);

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

TEST_CASE_FIXTURE(SyncedAnimationGraphFixture, "[SceneTree][SyncedAnimationGraph][BlendTree][Blend2Node] BlendTree evaluation with a Blend2Node connected to the output") {
	Ref<BLTAnimationNodeBlendTree> synced_blend_tree_node;
	synced_blend_tree_node.instantiate();

	// TestAnimationA
	Ref<BLTAnimationNodeSampler> animation_sampler_node_a;
	animation_sampler_node_a.instantiate();
	animation_sampler_node_a->animation_name = "animation_library/TestAnimationA";

	synced_blend_tree_node->add_node(animation_sampler_node_a);

	// TestAnimationB
	Ref<BLTAnimationNodeSampler> animation_sampler_node_b;
	animation_sampler_node_b.instantiate();
	animation_sampler_node_b->animation_name = "animation_library/TestAnimationB";

	synced_blend_tree_node->add_node(animation_sampler_node_b);

	// Blend2
	Ref<BLTAnimationNodeBlend2> blend2_node;
	blend2_node.instantiate();
	blend2_node->name = "Blend2";
	blend2_node->blend_weight = 0.5;
	blend2_node->sync = false;

	synced_blend_tree_node->add_node(blend2_node);

	// Connect nodes
	Vector<StringName> blend2_inputs;
	blend2_node->get_input_names(blend2_inputs);
	REQUIRE(synced_blend_tree_node->add_connection(animation_sampler_node_a, blend2_node, blend2_inputs[0]));
	REQUIRE(synced_blend_tree_node->add_connection(animation_sampler_node_b, blend2_node, blend2_inputs[1]));
	REQUIRE(synced_blend_tree_node->add_connection(blend2_node, synced_blend_tree_node->get_output_node(), "Input"));

	synced_blend_tree_node->initialize(synced_animation_graph->get_context());

	int blend2_node_index = synced_blend_tree_node->find_node_index(blend2_node);
	const BLTAnimationNodeBlendTree::NodeRuntimeData &blend2_runtime_data = synced_blend_tree_node->_node_runtime_data[blend2_node_index];

	CHECK(blend2_runtime_data.input_nodes[0] == animation_sampler_node_a);
	CHECK(blend2_runtime_data.input_nodes[1] == animation_sampler_node_b);

	synced_animation_graph->set_root_animation_node(synced_blend_tree_node);

	SUBCASE("Perform default evaluation") {
		Vector3 hip_bone_position = skeleton_node->get_bone_global_pose(hip_bone_index).origin;

		CHECK(hip_bone_position.x == doctest::Approx(0.0));
		CHECK(hip_bone_position.y == doctest::Approx(0.0));
		CHECK(hip_bone_position.z == doctest::Approx(0.0));

		SceneTree::get_singleton()->process(0.5);

		hip_bone_position = skeleton_node->get_bone_global_pose(hip_bone_index).origin;

		CHECK(hip_bone_position.x == doctest::Approx(0.75));
		CHECK(hip_bone_position.y == doctest::Approx(1.5));
		CHECK(hip_bone_position.z == doctest::Approx(2.25));
	}

	SUBCASE("Evaluate tree such that animations get looped") {
		Vector3 hip_bone_position = skeleton_node->get_bone_global_pose(hip_bone_index).origin;

		CHECK(hip_bone_position.x == doctest::Approx(0.0));
		CHECK(hip_bone_position.y == doctest::Approx(0.0));
		CHECK(hip_bone_position.z == doctest::Approx(0.0));

		SceneTree::get_singleton()->process(1.2);

		hip_bone_position = skeleton_node->get_bone_global_pose(hip_bone_index).origin;

		CHECK(hip_bone_position.x == doctest::Approx(0.3));
		CHECK(hip_bone_position.y == doctest::Approx(0.6));
		CHECK(hip_bone_position.z == doctest::Approx(0.9));
	}

	SUBCASE("Evaluate synced blend") {
		animation_sampler_node_a->animation_name = "animation_library/TestAnimationSyncA";
		animation_sampler_node_b->animation_name = "animation_library/TestAnimationSyncB";
		blend2_node->sync = true;
		synced_blend_tree_node->initialize(synced_animation_graph->get_context());

		REQUIRE(synced_animation_graph->get_root_animation_node().ptr() == synced_blend_tree_node.ptr());

		// By blending both animations we get a SyncTrack of duration 1.5s with the following
		// intervals:
		//   0: 0.825s
		//   1: 0.675s
		// By updating by 0s we get to the start of interval 0, an update by 0.65s to the start of interval 1, and
		// another update by 0.85 to the start again to interval 0.
		SceneTree::get_singleton()->process(0.);

		Vector3 hip_bone_position = skeleton_node->get_bone_global_pose(hip_bone_index).origin;

		CHECK(hip_bone_position.x == doctest::Approx(0.0));
		CHECK(hip_bone_position.y == doctest::Approx(0.0));
		CHECK(hip_bone_position.z == doctest::Approx(0.0));

		// By updating again by 0.825s we get loop to the start of the 0th interval where
		// both TrackValues are zero.
		SceneTree::get_singleton()->process(0.825);

		hip_bone_position = skeleton_node->get_bone_global_pose(hip_bone_index).origin;

		CHECK(hip_bone_position.x == doctest::Approx(1.5));
		CHECK(hip_bone_position.y == doctest::Approx(3.0));
		CHECK(hip_bone_position.z == doctest::Approx(4.5));

		// By updating again by 0.675s we loop to the start of the 0th interval where both
		// TrackValues are zero.
		SceneTree::get_singleton()->process(0.675);

		hip_bone_position = skeleton_node->get_bone_global_pose(hip_bone_index).origin;

		CHECK(hip_bone_position.x == doctest::Approx(0.0));
		CHECK(hip_bone_position.y == doctest::Approx(0.0));
		CHECK(hip_bone_position.z == doctest::Approx(0.0));
	}

	SUBCASE("Save, load and evaluate the SyncedBlendTree") {
		// Test saving and loading of the blend tree to a resource
		ResourceSaver::save(synced_blend_tree_node, "synced_blend_tree_node.tres");

		REQUIRE(ClassDB::class_exists("AnimationSamplerNode"));

		// Load blend tree
		Ref<BLTAnimationNodeBlendTree> loaded_synced_blend_tree = ResourceLoader::load("synced_blend_tree_node.tres");
		REQUIRE(loaded_synced_blend_tree.is_valid());

		Ref<BLTAnimationNodeBlend2> loaded_blend2_node = loaded_synced_blend_tree->get_node(loaded_synced_blend_tree->find_node_index_by_name("Blend2"));
		REQUIRE(loaded_blend2_node.is_valid());
		CHECK(loaded_blend2_node->sync == false);
		CHECK(loaded_blend2_node->blend_weight == blend2_node->blend_weight);

		loaded_synced_blend_tree->initialize(synced_animation_graph->get_context());
		synced_animation_graph->set_root_animation_node(loaded_synced_blend_tree);

		// Re-evaluate using a different time. All animation samplers will start again from 0.
		SceneTree::get_singleton()->process(0.2);

		Vector3 hip_bone_position = skeleton_node->get_bone_global_pose(hip_bone_index).origin;

		CHECK(hip_bone_position.x == doctest::Approx(0.3));
		CHECK(hip_bone_position.y == doctest::Approx(0.6));
		CHECK(hip_bone_position.z == doctest::Approx(0.9));
	}
}

} //namespace TestSyncedAnimationGraph