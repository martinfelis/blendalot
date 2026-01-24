//
// Created by martin on 03.12.25.
//

#include "blendalot_animation_node.h"

void BLTAnimationNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_position", "position"), &BLTAnimationNode::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &BLTAnimationNode::get_position);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position"), "set_position", "get_position");

	ADD_SIGNAL(MethodInfo("tree_changed"));
	ADD_SIGNAL(MethodInfo("animation_node_renamed", PropertyInfo(Variant::INT, "object_id"), PropertyInfo(Variant::STRING, "old_name"), PropertyInfo(Variant::STRING, "new_name")));
	ADD_SIGNAL(MethodInfo("animation_node_removed", PropertyInfo(Variant::INT, "object_id"), PropertyInfo(Variant::STRING, "name")));

	ClassDB::bind_method(D_METHOD("get_input_names"), &BLTAnimationNode::get_input_names_as_typed_array);
	ClassDB::bind_method(D_METHOD("get_input_count"), &BLTAnimationNode::get_input_count);
	ClassDB::bind_method(D_METHOD("get_input_index"), &BLTAnimationNode::get_input_index);
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

void BLTAnimationNode::_tree_changed() {
	emit_signal(SNAME("tree_changed"));
}

void BLTAnimationNode::_animation_node_renamed(const ObjectID &p_oid, const String &p_old_name, const String &p_new_name) {
	emit_signal(SNAME("animation_node_renamed"), p_oid, p_old_name, p_new_name);
}

void BLTAnimationNode::_animation_node_removed(const ObjectID &p_oid, const StringName &p_node) {
	emit_signal(SNAME("animation_node_removed"), p_oid, p_node);
}

void BLTAnimationNodeBlendTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_node", "animation_node"), &BLTAnimationNodeBlendTree::add_node);
	ClassDB::bind_method(D_METHOD("get_node", "node_name"), &BLTAnimationNodeBlendTree::get_node);
	ClassDB::bind_method(D_METHOD("get_output_node"), &BLTAnimationNodeBlendTree::get_output_node);
	ClassDB::bind_method(D_METHOD("get_node_names"), &BLTAnimationNodeBlendTree::get_node_names_as_typed_array);

	ClassDB::bind_method(D_METHOD("is_connection_valid", "source_node", "target_node", "target_port_name"), &BLTAnimationNodeBlendTree::is_connection_valid);
	ClassDB::bind_method(D_METHOD("add_connection", "source_node", "target_node", "target_port_name"), &BLTAnimationNodeBlendTree::add_connection);
	ClassDB::bind_method(D_METHOD("get_connections"), &BLTAnimationNodeBlendTree::get_connections_as_array);

	BIND_CONSTANT(CONNECTION_OK);
	BIND_CONSTANT(CONNECTION_ERROR_GRAPH_ALREADY_INITIALIZED);
	BIND_CONSTANT(CONNECTION_ERROR_NO_SOURCE_NODE);
	BIND_CONSTANT(CONNECTION_ERROR_NO_TARGET_NODE);
	BIND_CONSTANT(CONNECTION_ERROR_PARENT_EXISTS);
	BIND_CONSTANT(CONNECTION_ERROR_TARGET_PORT_NOT_FOUND);
	BIND_CONSTANT(CONNECTION_ERROR_TARGET_PORT_ALREADY_CONNECTED);
	BIND_CONSTANT(CONNECTION_ERROR_CONNECTION_CREATES_LOOP);
}

void BLTAnimationNodeBlendTree::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const Ref<BLTAnimationNode> &node : tree_graph.nodes) {
		String prop_name = node->get_name();
		if (prop_name != "Output") {
			p_list->push_back(PropertyInfo(Variant::OBJECT, "nodes/" + prop_name + "/node", PROPERTY_HINT_RESOURCE_TYPE, "AnimationNode", PROPERTY_USAGE_NO_EDITOR));
		}
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "nodes/" + prop_name + "/position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	}

	p_list->push_back(PropertyInfo(Variant::ARRAY, "node_connections", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
}

bool BLTAnimationNodeBlendTree::_get(const StringName &p_name, Variant &r_value) const {
	String prop_name = p_name;
	if (prop_name.begins_with("nodes/")) {
		String node_name = prop_name.get_slicec('/', 1);
		String what = prop_name.get_slicec('/', 2);
		int node_index = find_node_index_by_name(node_name);

		if (what == "node") {
			if (node_index != -1) {
				r_value = tree_graph.nodes[node_index];
				return true;
			}
		}

		if (what == "position") {
			if (node_index != -1) {
				r_value = tree_graph.nodes[node_index]->position;
				return true;
			}
		}
	} else if (prop_name == "node_connections") {
		Array conns;
		conns.resize(tree_graph.connections.size() * 3);

		int idx = 0;
		for (const BLTBlendTreeConnection &connection : tree_graph.connections) {
			conns[idx * 3 + 0] = connection.target_node->get_name();
			conns[idx * 3 + 1] = connection.target_node->get_input_index(connection.target_port_name);
			conns[idx * 3 + 2] = connection.source_node->get_name();
			idx++;
		}

		r_value = conns;
		return true;
	}

	return false;
}

bool BLTAnimationNodeBlendTree::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name;
	if (prop_name.begins_with("nodes/")) {
		String node_name = prop_name.get_slicec('/', 1);
		String what = prop_name.get_slicec('/', 2);

		if (what == "node") {
			Ref<BLTAnimationNode> anode = p_value;
			if (anode.is_valid()) {
				anode->set_name(node_name);
				add_node(anode);
			}
			return true;
		}

		if (what == "position") {
			int node_index = find_node_index_by_name(node_name);
			if (node_index > -1) {
				tree_graph.nodes[node_index]->position = p_value;
			}
			return true;
		}
	} else if (prop_name == "node_connections") {
		Array conns = p_value;
		ERR_FAIL_COND_V(conns.size() % 3 != 0, false);

		for (int i = 0; i < conns.size(); i += 3) {
			int target_node_index = find_node_index_by_name(conns[i]);
			int target_node_port_index = conns[i + 1];
			int source_node_index = find_node_index_by_name(conns[i + 2]);

			Ref<BLTAnimationNode> target_node = tree_graph.nodes[target_node_index];
			Vector<StringName> target_input_names = target_node->get_input_names();

			add_connection(tree_graph.nodes[source_node_index], target_node, target_input_names[target_node_port_index]);
		}
		return true;
	}

	return false;
}

void AnimationData::sample_from_animation(const Ref<Animation> &animation, const Skeleton3D *skeleton_3d, double p_time) {
	GodotProfileZone("AnimationData::sample_from_animation");

	const LocalVector<Animation::Track *> tracks = animation->get_tracks();
	Animation::Track *const *tracks_ptr = tracks.ptr();

	int count = tracks.size();
	for (int i = 0; i < count; i++) {
		const Animation::Track *animation_track = tracks_ptr[i];
		if (!animation_track->enabled) {
			continue;
		}

		Animation::TrackType ttype = animation_track->type;
		switch (ttype) {
			case Animation::TYPE_POSITION_3D:
			case Animation::TYPE_ROTATION_3D: {
				TransformTrackValue *transform_track_value = get_value<TransformTrackValue>(animation_track->thash);

				if (transform_track_value->bone_idx != -1) {
					switch (ttype) {
						case Animation::TYPE_POSITION_3D: {
							animation->try_position_track_interpolate(i, p_time, &transform_track_value->loc);
							transform_track_value->loc_used = true;
							break;
						}
						case Animation::TYPE_ROTATION_3D: {
							animation->try_rotation_track_interpolate(i, p_time, &transform_track_value->rot);
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

void AnimationData::allocate_track_value(const Animation::Track *animation_track, const Skeleton3D *skeleton_3d) {
	switch (animation_track->type) {
		case Animation::TrackType::TYPE_ROTATION_3D:
		case Animation::TrackType::TYPE_POSITION_3D: {
			size_t value_offset = 0;
			AnimationData::TransformTrackValue *transform_track_value = nullptr;
			if (value_buffer_offset.has(animation_track->thash)) {
				value_offset = value_buffer_offset[animation_track->thash];
				transform_track_value = reinterpret_cast<AnimationData::TransformTrackValue *>(&buffer[value_offset]);
			} else {
				value_offset = buffer.size();
				value_buffer_offset.insert(animation_track->thash, buffer.size());
				buffer.resize(buffer.size() + sizeof(AnimationData::TransformTrackValue));
				transform_track_value = new (reinterpret_cast<AnimationData::TransformTrackValue *>(&buffer[value_offset])) AnimationData::TransformTrackValue();
			}
			assert(transform_track_value != nullptr);
			if (animation_track->path.get_subname_count() == 1) {
				transform_track_value->bone_idx = skeleton_3d->find_bone(animation_track->path.get_subname(0));
			}

			if (animation_track->type == Animation::TrackType::TYPE_POSITION_3D) {
				transform_track_value->loc_used = true;
			} else if (animation_track->type == Animation::TrackType::TYPE_ROTATION_3D) {
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

	const LocalVector<Animation::Track *> tracks = animation->get_tracks();
	Animation::Track *const *tracks_ptr = tracks.ptr();

	int count = tracks.size();
	for (int i = 0; i < count; i++) {
		const Animation::Track *animation_track = tracks_ptr[i];
		if (!animation_track->enabled) {
			continue;
		}

		allocate_track_value(animation_track, skeleton_3d);
	}
}

void AnimationDataAllocator::register_track_values(const Ref<Animation> &animation, const Skeleton3D *skeleton_3d) {
	default_data.allocate_track_values(animation, skeleton_3d);
}

bool BLTAnimationNodeSampler::initialize(GraphEvaluationContext &context) {
	if (!BLTAnimationNode::initialize(context)) {
		return false;
	}

	animation = context.animation_player->get_animation(animation_name);
	if (!animation.is_valid()) {
		print_error(vformat("Cannot initialize node %s: animation '%s' not found in animation player.", get_name(), animation_name));
		return false;
	}

	context.animation_data_allocator.register_track_values(animation, context.skeleton_3d);

	node_time_info.loop_mode = animation->get_loop_mode();

	// Initialize Sync Track from marker
	LocalVector<float> sync_markers;
	int marker_index = 0;
	StringName marker_name = itos(marker_index);
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

	return true;
}

void BLTAnimationNodeSampler::update_time(double p_time) {
	BLTAnimationNode::update_time(p_time);

	if (node_time_info.is_synced) {
		// Any potential looping has already been performed in the sync-controlling node.
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

	if (node_time_info.is_synced) {
		node_time_info.position = node_time_info.sync_track.calc_ratio_from_sync_time(node_time_info.sync_position) * animation->get_length();
	}

	output.sample_from_animation(animation, context.skeleton_3d, node_time_info.position);
}

void BLTAnimationNodeSampler::set_animation(const StringName &p_name) {
	animation_name = p_name;
}

StringName BLTAnimationNodeSampler::get_animation() const {
	return animation_name;
}

void BLTAnimationNodeSampler::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_animation", "name"), &BLTAnimationNodeSampler::set_animation);
	ClassDB::bind_method(D_METHOD("get_animation"), &BLTAnimationNodeSampler::get_animation);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "animation"), "set_animation", "get_animation");
}

void BLTAnimationNodeBlend2::evaluate(GraphEvaluationContext &context, const LocalVector<AnimationData *> &inputs, AnimationData &output) {
	GodotProfileZone("AnimationBlend2Node::evaluate");

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
	p_list->push_back(PropertyInfo(Variant::BOOL, sync_pname));
}

bool BLTAnimationNodeBlend2::_get(const StringName &p_name, Variant &r_value) const {
	if (p_name == blend_weight_pname) {
		r_value = blend_weight;
		return true;
	}

	if (p_name == sync_pname) {
		r_value = sync;
		return true;
	}

	return false;
}

bool BLTAnimationNodeBlend2::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == blend_weight_pname) {
		blend_weight = p_value;
		return true;
	}

	if (p_name == sync_pname) {
		sync = p_value;
		return true;
	}

	return false;
}

BLTAnimationNodeBlendTree::BLTBlendTreeGraph::BLTBlendTreeGraph() {
	Ref<BLTAnimationNodeOutput> output_node;
	output_node.instantiate();
	output_node->set_name("Output");
	add_node(output_node);
}

Ref<BLTAnimationNode> BLTAnimationNodeBlendTree::BLTBlendTreeGraph::get_output_node() {
	return nodes[0];
}

int BLTAnimationNodeBlendTree::BLTBlendTreeGraph::find_node_index(const Ref<BLTAnimationNode> &node) const {
	for (int i = 0; i < nodes.size(); i++) {
		if (nodes[i] == node) {
			return i;
		}
	}

	return -1;
}

int BLTAnimationNodeBlendTree::BLTBlendTreeGraph::find_node_index_by_name(const StringName &name) const {
	for (int i = 0; i < nodes.size(); i++) {
		if (nodes[i]->get_name() == name) {
			return i;
		}
	}

	return -1;
}

void BLTAnimationNodeBlendTree::BLTBlendTreeGraph::add_node(const Ref<BLTAnimationNode> &node) {
	StringName node_base_name = node->get_name();
	if (node_base_name.is_empty()) {
		node_base_name = node->get_class_name();
	}
	node->set_name(node_base_name);

	int number_suffix = 1;
	while (find_node_index_by_name(node->get_name()) != -1) {
		node->set_name(vformat("%s %d", node_base_name, number_suffix));
		number_suffix++;
	}

	nodes.push_back(node);
	node_connection_info.push_back(NodeConnectionInfo(node.ptr()));
}

void BLTAnimationNodeBlendTree::BLTBlendTreeGraph::sort_nodes_and_references() {
	LocalVector<int> sorted_node_indices = get_sorted_node_indices();

	Vector<Ref<BLTAnimationNode>> sorted_nodes;
	LocalVector<NodeConnectionInfo> old_node_connection_info = node_connection_info;
	for (unsigned int i = 0; i < sorted_node_indices.size(); i++) {
		int node_index = sorted_node_indices[i];
		sorted_nodes.push_back(nodes[node_index]);
		node_connection_info[i] = old_node_connection_info[node_index];
	}
	nodes = sorted_nodes;

	for (NodeConnectionInfo &connection_info : node_connection_info) {
		if (connection_info.parent_node_index != -1) {
			connection_info.parent_node_index = sorted_node_indices[connection_info.parent_node_index];
		}
		connection_info.apply_node_mapping(sorted_node_indices);
	}
}

LocalVector<int> BLTAnimationNodeBlendTree::BLTBlendTreeGraph::get_sorted_node_indices() {
	LocalVector<int> result;

	sort_nodes_recursive(0, result);
	result.reverse();

	return result;
}

void BLTAnimationNodeBlendTree::BLTBlendTreeGraph::sort_nodes_recursive(int node_index, LocalVector<int> &result) {
	for (int input_node_index : node_connection_info[node_index].connected_child_node_index_at_port) {
		if (input_node_index >= 0) {
			sort_nodes_recursive(input_node_index, result);
		}
	}
	result.push_back(node_index);
}

void BLTAnimationNodeBlendTree::BLTBlendTreeGraph::add_index_and_update_subtrees_recursive(int node, int node_parent) {
	if (node_parent == -1) {
		return;
	}

	node_connection_info[node_parent].input_subtree_node_indices.insert(node);

	for (int index : node_connection_info[node].input_subtree_node_indices) {
		node_connection_info[node_parent].input_subtree_node_indices.insert(index);
	}

	add_index_and_update_subtrees_recursive(node_parent, node_connection_info[node_parent].parent_node_index);
}

BLTAnimationNodeBlendTree::ConnectionError BLTAnimationNodeBlendTree::BLTBlendTreeGraph::add_connection(const Ref<BLTAnimationNode> &source_node, const Ref<BLTAnimationNode> &target_node, const StringName &target_port_name) {
	ConnectionError result = is_connection_valid(source_node, target_node, target_port_name);
	if (result != CONNECTION_OK) {
		return result;
	}

	int source_node_index = find_node_index(source_node);
	int target_node_index = find_node_index(target_node);
	int target_input_port_index = target_node->get_input_index(target_port_name);

	node_connection_info[source_node_index].parent_node_index = target_node_index;
	node_connection_info[target_node_index].connected_child_node_index_at_port[target_input_port_index] = source_node_index;
	connections.push_back(BLTBlendTreeConnection{ source_node, target_node, target_port_name });

	add_index_and_update_subtrees_recursive(source_node_index, target_node_index);

	return CONNECTION_OK;
}

BLTAnimationNodeBlendTree::ConnectionError BLTAnimationNodeBlendTree::BLTBlendTreeGraph::is_connection_valid(const Ref<BLTAnimationNode> &source_node, const Ref<BLTAnimationNode> &target_node, StringName target_port_name) const {
	int source_node_index = find_node_index(source_node);
	if (source_node_index == -1) {
		print_error("Cannot connect nodes: source node not found.");
		return CONNECTION_ERROR_NO_SOURCE_NODE;
	}

	if (node_connection_info[source_node_index].parent_node_index != -1) {
		print_error("Cannot connect node: source node already has a parent.");
		return CONNECTION_ERROR_PARENT_EXISTS;
	}

	int target_node_index = find_node_index(target_node);
	if (target_node_index == -1) {
		print_error("Cannot connect nodes: target node not found.");
		return CONNECTION_ERROR_NO_TARGET_NODE;
	}

	Vector<StringName> target_inputs = target_node->get_input_names();

	if (!target_inputs.has(target_port_name)) {
		print_error("Cannot connect nodes: target port not found.");
		return CONNECTION_ERROR_TARGET_PORT_NOT_FOUND;
	}

	int target_input_port_index = target_node->get_input_index(target_port_name);
	if (node_connection_info[target_node_index].connected_child_node_index_at_port[target_input_port_index] != -1) {
		print_error("Cannot connect node: target port already connected");
		return CONNECTION_ERROR_TARGET_PORT_ALREADY_CONNECTED;
	}

	if (node_connection_info[source_node_index].input_subtree_node_indices.has(target_node_index)) {
		print_error("Cannot connect node: connection would create loop.");
		return CONNECTION_ERROR_CONNECTION_CREATES_LOOP;
	}

	return CONNECTION_OK;
}
