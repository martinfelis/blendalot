@tool

extends Control
class_name BltBlendTreeEditor

@onready var blend_tree_graph_edit: GraphEdit = %BlendTreeGraphEdit
@onready var add_node_popup_menu: PopupMenu = %AddNodePopupMenu

signal edit_subgraph(blt_node:BLTAnimationNode)
signal graph_changed()

var blend_tree:BLTBlendTree

var blend_tree_node_to_graph_node = {}
var graph_node_to_blend_tree_node = {}

var selected_nodes = {}
var last_selected_graph_node:GraphNode = null
var new_node_position:Vector2 = Vector2.ZERO

var registered_nodes = [ 
	"BLTAnimationNodeSampler",
	"BLTAnimationNodeBlend2",
	"BLTAnimationNodeTimeScale",
	"BLTBlendTree",
	"BLTStateMachine"
	]


func _ready() -> void:
	add_node_popup_menu.clear(true)
	
	for node_name in registered_nodes:
		add_node_popup_menu.add_item(node_name)


func _reset_editor():
	for child in blend_tree_graph_edit.get_children():
		if child.name == "_connection_layer":
			continue
			
		child.get_parent().remove_child(child)
		child.queue_free()
	
	blend_tree_graph_edit.clear_connections()
	
	blend_tree = null
	blend_tree_node_to_graph_node = {}
	graph_node_to_blend_tree_node = {}
	selected_nodes = {}
	

func edit_blend_tree(blt_blend_tree:BLTBlendTree):
	_reset_editor()
	blend_tree = blt_blend_tree
	blend_tree_graph_edit.scroll_offset = blend_tree.graph_offset
	
	_update_editor_nodes_from_blend_tree()
	_update_editor_connections_from_blend_tree()


func _update_editor_nodes_from_blend_tree():
	for node_name in blend_tree.get_node_names():
		var blend_tree_node:BLTAnimationNode = blend_tree.get_node(node_name)
		var graph_node:GraphNode = create_graph_node_for_blt_node(blend_tree_node)
		blend_tree_graph_edit.add_child(graph_node)
		
		blend_tree_node_to_graph_node[blend_tree_node] = graph_node
		graph_node_to_blend_tree_node[graph_node] = blend_tree_node


func _update_editor_connections_from_blend_tree():
	var connection_array = blend_tree.get_connections()

	for i in range(len(connection_array) / 3):
		var source_node:BLTAnimationNode = connection_array[i * 3]
		var target_node:BLTAnimationNode = connection_array[i * 3 + 1]
		var target_port = connection_array[i * 3 + 2]
		
		var source_graph_node = blend_tree_node_to_graph_node[source_node]
		
		var connect_result = blend_tree_graph_edit.connect_node(source_node.resource_name, 0, target_node.resource_name, target_node.get_input_index(target_port), true)


func create_graph_node_for_blt_node(blt_node: BLTAnimationNode) -> GraphNode:
	var result_graph_node:GraphNode = GraphNode.new()
	result_graph_node.name = blt_node.resource_name
	result_graph_node.title = blt_node.resource_name
	result_graph_node.position_offset = blt_node.position
	
	var result_slot_offset = 0
	
	if (blt_node.get_class() != "BLTAnimationNodeOutput"):
		result_slot_offset = 1
		var output_slot_label:Label = Label.new()
		output_slot_label.text = "Result"
		result_graph_node.add_child(output_slot_label)
		result_graph_node.set_slot(0, false, 1, Color.WHITE, true, 1, Color.WHITE)	
	
	if blt_node.get_class() == "BLTBlendTree" or blt_node.get_class() == "BLTStateMachine":
		result_graph_node.gui_input.connect(_on_node_gui_input.bind(result_graph_node))
	
	var inputs = blt_node.get_input_names()
	for i in range(len(inputs)):
		var slot_label:Label = Label.new()
		slot_label.text = inputs[i]
		result_graph_node.add_child(slot_label)
		result_graph_node.set_slot(i + result_slot_offset, true, 1, Color.WHITE, false, 1, Color.BLACK)
	
	if blt_node.get_class() == "BLTAnimationNodeSampler":
		var animation_sampler_node:BLTAnimationNodeSampler = blt_node as BLTAnimationNodeSampler
		var animation_selector_button = OptionButton.new()
		result_graph_node.add_child(animation_selector_button)

		var animation_player:AnimationPlayer = animation_sampler_node.get_animation_player()
		
		if is_instance_valid(animation_player):
			for animation_name in animation_player.get_animation_list():
				animation_selector_button.add_item(animation_name)
				if animation_name == animation_sampler_node.animation:
					animation_selector_button.select(animation_selector_button.item_count - 1)
					
			animation_selector_button.item_selected.connect(_on_animation_select.bind(animation_sampler_node, animation_selector_button))
	
	blt_node.node_changed.connect(_trigger_graph_changed)
	
	return result_graph_node


func _trigger_graph_changed(_node_name):
	graph_changed.emit()


func _remove_node_connections(graph_node:GraphNode):
	var node_connections:Array = []
	
	for connection:Dictionary in blend_tree_graph_edit.connections:
		if connection["from_node"] == graph_node.name or connection["to_node"] == graph_node.name:
			node_connections.append(connection)
	
	for node_connection:Dictionary in node_connections:
		print("Removing connection %s" % str(node_connection))
		blend_tree_graph_edit.disconnect_node(node_connection["from_node"], node_connection["from_port"], node_connection["to_node"], node_connection["to_port"])


#
# GraphEdit signal handling
#
func _on_blend_tree_graph_edit_connection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	var source_node:BLTAnimationNode = blend_tree.get_node(from_node)
	var target_node:BLTAnimationNode = blend_tree.get_node(to_node)
	
	if target_node == null:
		push_error("Invalid connection, target node %s not found." % to_node)
		return
	
	var target_node_port_name = target_node.get_input_names()[to_port]
	
	var connection_result = blend_tree.is_connection_valid(source_node, target_node, target_node_port_name)
	if connection_result != blend_tree.CONNECTION_OK:
		push_error("Could not add connection (error %d)" % connection_result)
		return
	
	blend_tree.add_connection(source_node, target_node, target_node_port_name)
	
	var connect_result = blend_tree_graph_edit.connect_node(from_node, from_port, to_node, to_port, true)


func _on_blend_tree_graph_edit_disconnection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	var blend_tree_source_node = blend_tree.get_node(from_node)
	var blend_tree_target_node = blend_tree.get_node(to_node)
	var target_port_name = blend_tree_target_node.get_input_names()[to_port]
	blend_tree.remove_connection(blend_tree_source_node, blend_tree_target_node, target_port_name)
	
	blend_tree_graph_edit.disconnect_node(from_node, from_port, to_node, to_port)


func _on_blend_tree_graph_edit_delete_nodes_request(nodes: Array[StringName]) -> void:
	for node_name:StringName in nodes:
		print("remove node '%s'" % node_name)
		var blend_tree_node:BLTAnimationNode = blend_tree.get_node(node_name)
		
		if blend_tree_node == null:
			push_error("Cannot delete node '%s': node not found." % node_name)
			continue
		
		if blend_tree_node == blend_tree.get_output_node():
			push_warning("Output node not allowed to be removed.")
			continue
		
		blend_tree_node.node_changed.disconnect(_trigger_graph_changed)
		
		var graph_node:GraphNode = blend_tree_node_to_graph_node[blend_tree_node]
		blend_tree.remove_node(blend_tree_node)
		blend_tree_node_to_graph_node.erase(blend_tree_node)
		
		_remove_node_connections(graph_node)
		graph_node_to_blend_tree_node.erase(graph_node)
		blend_tree_graph_edit.remove_child(graph_node)
		_on_blend_tree_graph_edit_node_deselected(graph_node)
		
		EditorInterface.get_inspector().edit(null)


func _on_blend_tree_graph_edit_end_node_move() -> void:
	for graph_node:GraphNode in selected_nodes.keys():
		graph_node_to_blend_tree_node[graph_node].position = graph_node.position_offset


func _on_blend_tree_graph_edit_node_deselected(graph_node: Node) -> void:
	if selected_nodes.has(graph_node):
		selected_nodes.erase(graph_node)


func _on_blend_tree_graph_edit_node_selected(graph_node: Node) -> void:
	selected_nodes[graph_node] = graph_node
	last_selected_graph_node = graph_node
	EditorInterface.get_inspector().edit(graph_node_to_blend_tree_node[graph_node])


func _on_blend_tree_graph_edit_scroll_offset_changed(offset: Vector2) -> void:
	if is_instance_valid(blend_tree):
		blend_tree.graph_offset = offset


#
# AddNodePopupMenu
#
func _on_blend_tree_graph_edit_popup_request(at_position: Vector2) -> void:
	add_node_popup_menu.position = get_screen_position() + get_local_mouse_position()
	add_node_popup_menu.reset_size()
	add_node_popup_menu.popup()
	new_node_position = (get_local_mouse_position() + blend_tree_graph_edit.scroll_offset) / blend_tree_graph_edit.zoom


func _on_add_node_popup_menu_index_pressed(index: int) -> void:
	var new_blend_tree_node: BLTAnimationNode = ClassDB.instantiate(registered_nodes[index])
	blend_tree.add_node(new_blend_tree_node)	
	
	var graph_node:GraphNode = create_graph_node_for_blt_node(new_blend_tree_node)
	blend_tree_graph_edit.add_child(graph_node)
	
	graph_node_to_blend_tree_node[graph_node] = new_blend_tree_node
	blend_tree_node_to_graph_node[new_blend_tree_node] = graph_node
	
	if new_node_position != Vector2.INF:
		graph_node.position_offset = new_node_position
		new_blend_tree_node.position = graph_node.position_offset
	
	new_node_position = Vector2.INF


#
# Handle Node double click
#
func _on_node_gui_input(input_event:InputEvent, graph_node:GraphNode):
	# print("Got input event on graph node %s!" % graph_node.name)
	
	var mouse_button_event:InputEventMouseButton = input_event as InputEventMouseButton
	if mouse_button_event and mouse_button_event.double_click:
		_on_node_double_click(graph_node)

func _on_node_double_click(graph_node:GraphNode):
	var blend_tree_node:BLTAnimationNode = graph_node_to_blend_tree_node[graph_node]
	
	if blend_tree_node is BLTBlendTree:
		edit_subgraph.emit(blend_tree_node)
	elif blend_tree_node is BLTStateMachine:
		edit_subgraph.emit(blend_tree_node)

#
# Animation selection for BltAnimationNodeSampler
#
func _on_animation_select(index:int, blt_node_sampler:BLTAnimationNodeSampler, option_button:OptionButton):
	blt_node_sampler.animation = option_button.get_item_text(index)
	blt_node_sampler.node_changed.emit(blt_node_sampler.resource_name)
