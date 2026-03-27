@tool

extends Control
class_name BltStateMachineEditor

@onready var registered_nodes = load ("res://addons/blendalot/blendalot_node_registry.gd").new().registered_nodes

@onready var state_machine_graph_edit: StateMachineGraphEdit = %StateMachineGraphEdit
@onready var add_state_popup_menu: PopupMenu = %AddStatePopupMenu

signal edit_subgraph(blt_node:BLTAnimationNode)
signal graph_changed()

var state_machine:BLTStateMachine

var state_node_to_graph_node = {}
var graph_node_to_state_node = {}
var transition_lines = []

var selected_nodes = {}
var last_selected_graph_node:GraphElement = null
var new_state_position:Vector2 = Vector2.ZERO
var transition_trag_start_position:Vector2 = Vector2.INF


func _ready() -> void:
	add_state_popup_menu.clear(true)
	
	for node_name in registered_nodes:
		add_state_popup_menu.add_item(node_name)


func _reset_editor():
	for child in state_machine_graph_edit.get_children():
		if child.name == "_connection_layer":
			continue
			
		child.get_parent().remove_child(child)
		child.queue_free()
	
	state_machine_graph_edit.clear_connections()
	
	state_machine = null
	state_node_to_graph_node = {}
	graph_node_to_state_node = {}
	selected_nodes = {}


func edit_state_machine(blt_state_machine:BLTStateMachine):
	_reset_editor()
	state_machine = blt_state_machine
	state_machine_graph_edit.scroll_offset = state_machine.graph_offset
	
	_update_editor_nodes_from_state_machine()
	_update_editor_connections_from_state_machine()


func _update_editor_nodes_from_state_machine():
	for state_name in state_machine.get_state_names():
		var state_node:BLTAnimationNode = state_machine.get_node(state_name)
		var graph_node:GraphElement = create_graph_node_for_blt_node(state_node)
		state_machine_graph_edit.add_child(graph_node)
		
		state_node_to_graph_node[state_node] = graph_node
		graph_node_to_state_node[graph_node] = state_node


func create_graph_node_for_blt_node(blt_node: BLTAnimationNode) -> GraphElement:
	var result_graph_node:GraphElement = GraphElement.new()
	result_graph_node.name = blt_node.resource_name
	var state_graph_element:StateMachineState = preload("res://addons/blendalot/state_machine_state.tscn").instantiate()
	result_graph_node.add_child(state_graph_element)
	state_graph_element.title = blt_node.resource_name
	state_graph_element.transition_border_size = 10
	result_graph_node.position_offset = blt_node.position
	
	blt_node.node_changed.connect(_trigger_graph_changed)
	
	result_graph_node.gui_input.connect(_on_node_gui_input.bind(result_graph_node))
	
	return result_graph_node


func _update_editor_connections_from_state_machine():
	var transitions_array = state_machine.get_transitions()

	var success:bool = true
	for i in range(len(transitions_array) / 3):
		var from_state:BLTAnimationNode = transitions_array[i * 3]
		var to_state:BLTAnimationNode = transitions_array[i * 3 + 1]
		var transition:BLTStateMachineTransition = transitions_array[i * 3 + 2]
		
		var from_state_node = state_node_to_graph_node[from_state]
		
		var connect_result = state_machine_graph_edit.connect_node(from_state.resource_name, 0, to_state.resource_name, 0, true)
		if not connect_result:
			success = false


func _trigger_graph_changed(_node_name):
	graph_changed.emit()


func _remove_node_connections(graph_node:GraphElement):
	var node_connections:Array = []
	
	for connection:Dictionary in state_machine_graph_edit.connections:
		if connection["from_node"] == graph_node.name or connection["to_node"] == graph_node.name:
			node_connections.append(connection)
	
	for node_connection:Dictionary in node_connections:
		print("Removing connection %s" % str(node_connection))
		state_machine_graph_edit.disconnect_node(node_connection["from_node"], node_connection["from_port"], node_connection["to_node"], node_connection["to_port"])


#
# GraphEdit signal handling
#
func _on_state_machine_graph_edit_connection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	var source_node:BLTAnimationNode = state_machine.get_node(from_node)
	var target_node:BLTAnimationNode = state_machine.get_node(to_node)
	
	if target_node == null:
		push_error("Invalid connection, target node %s not found." % to_node)
		return
	
	var target_node_port_name = target_node.get_input_names()[to_port]
	
	var connection_result = state_machine.is_connection_valid(source_node, target_node, target_node_port_name)
	if connection_result != state_machine.CONNECTION_OK:
		push_error("Could not add connection (error %d)" % connection_result)
		return
	
	state_machine.add_connection(source_node, target_node, target_node_port_name)
	
	var connect_result = state_machine_graph_edit.connect_node(from_node, from_port, to_node, to_port, true)


func _on_state_machine_graph_edit_disconnection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	var state_machine_source_node = state_machine.get_node(from_node)
	var state_machine_target_node = state_machine.get_node(to_node)
	var target_port_name = state_machine_target_node.get_input_names()[to_port]
	state_machine.remove_connection(state_machine_source_node, state_machine_target_node, target_port_name)
	
	state_machine_graph_edit.disconnect_node(from_node, from_port, to_node, to_port)


func _on_state_machine_graph_edit_delete_nodes_request(nodes: Array[StringName]) -> void:
	for node_name:StringName in nodes:
		print("remove node '%s'" % node_name)
		var state_machine_node:BLTAnimationNode = state_machine.get_node(node_name)
		
		if state_machine_node == null:
			push_error("Cannot delete node '%s': node not found." % node_name)
			continue
		
		if state_machine_node == state_machine.get_output_node():
			push_warning("Output node not allowed to be removed.")
			continue
		
		state_machine_node.node_changed.disconnect(_trigger_graph_changed)
		
		var graph_node:GraphElement = state_node_to_graph_node[state_machine_node]
		state_machine.remove_node(state_machine_node)
		state_node_to_graph_node.erase(state_machine_node)
		
		_remove_node_connections(graph_node)
		graph_node_to_state_node.erase(graph_node)
		state_machine_graph_edit.remove_child(graph_node)
		_on_state_machine_graph_edit_node_deselected(graph_node)
		
		EditorInterface.get_inspector().edit(null)


func _on_state_machine_graph_edit_end_node_move() -> void:
	for graph_node:GraphElement in selected_nodes.keys():
		graph_node_to_state_node[graph_node].position = graph_node.position_offset
	
	state_machine_graph_edit.queue_redraw()


func _on_state_machine_graph_edit_node_deselected(graph_node: Node) -> void:
	if selected_nodes.has(graph_node):
		selected_nodes.erase(graph_node)


func _on_state_machine_graph_edit_node_selected(graph_node: Node) -> void:
	selected_nodes[graph_node] = graph_node
	last_selected_graph_node = graph_node
	EditorInterface.get_inspector().edit(graph_node_to_state_node[graph_node])


func _on_state_machine_graph_edit_scroll_offset_changed(offset: Vector2) -> void:
	if is_instance_valid(state_machine):
		state_machine.graph_offset = offset
		
		state_machine_graph_edit.queue_redraw()


#
# AddNodePopupMenu
#
func _on_state_machine_graph_edit_popup_request(at_position: Vector2) -> void:
	add_state_popup_menu.position = get_screen_position() + get_local_mouse_position()
	add_state_popup_menu.reset_size()
	add_state_popup_menu.popup()
	new_state_position = (get_local_mouse_position() + state_machine_graph_edit.scroll_offset) / state_machine_graph_edit.zoom


func _on_add_state_popup_menu_index_pressed(index: int) -> void:
	var new_state_node: BLTAnimationNode = ClassDB.instantiate(registered_nodes[index])
	state_machine.add_state(new_state_node)	
	
	var graph_node:GraphElement = create_graph_node_for_blt_node(new_state_node)
	state_machine_graph_edit.add_child(graph_node)
	
	graph_node_to_state_node[graph_node] = new_state_node
	state_node_to_graph_node[new_state_node] = graph_node
	
	if new_state_position != Vector2.INF:
		graph_node.position_offset = new_state_position
		new_state_node.position = graph_node.position_offset
	
	new_state_position = Vector2.INF


#
# General input handling
#
func _on_gui_input(event: InputEvent) -> void:
	if transition_trag_start_position != Vector2.INF:
		return
		
	var mouse_button_event:InputEventMouseButton = event as InputEventMouseButton
	if not is_instance_valid(mouse_button_event):
		return
	
	if mouse_button_event.button_mask & MOUSE_BUTTON_LEFT == 0:
		print("drag end")
		transition_trag_start_position = Vector2.INF


#
# Handle Node double click
#
func _on_node_gui_input(input_event:InputEvent, graph_node:GraphElement):
	# print("Got input event on graph node %s!" % graph_node.name)
	
	var mouse_button_event:InputEventMouseButton = input_event as InputEventMouseButton
	if mouse_button_event and mouse_button_event.double_click:
		_on_node_double_click(graph_node)
		get_viewport().set_input_as_handled()
		return
	
	if Input.is_key_pressed(KEY_CTRL) and mouse_button_event and mouse_button_event.pressed and mouse_button_event.button_index == MouseButton.MOUSE_BUTTON_LEFT:
		print("drag_start")
		transition_trag_start_position = state_machine_graph_edit.get_mouse_graph_position()


func _on_node_double_click(graph_node:GraphElement):
	var state_machine_node:BLTAnimationNode = graph_node_to_state_node[graph_node]
	
	if state_machine_node is BLTBlendTree:
		edit_subgraph.emit(state_machine_node)
	elif state_machine_node is BLTStateMachine:
		edit_subgraph.emit(state_machine_node)

#
# Animation selection for BltAnimationNodeSampler
#
func _on_animation_select(index:int, blt_node_sampler:BLTAnimationNodeSampler, option_button:OptionButton):
	blt_node_sampler.animation = option_button.get_item_text(index)
	blt_node_sampler.node_changed.emit(blt_node_sampler.resource_name)


#
# Testdata
#
func _create_and_add_state(blt_node_type_name:String, name:String = "Test State", position:Vector2 = Vector2.ZERO):
	var new_state_node: BLTAnimationNode = ClassDB.instantiate(blt_node_type_name)
	new_state_node.resource_name = name
	new_state_node.position = position
	state_machine.add_state(new_state_node)	
	
	var graph_node:GraphElement = create_graph_node_for_blt_node(new_state_node)
	state_machine_graph_edit.add_child(graph_node)
	
	graph_node_to_state_node[graph_node] = new_state_node
	state_node_to_graph_node[new_state_node] = graph_node

	return new_state_node


func _add_transition_lines(from_state:BLTAnimationNode, to_state:BLTAnimationNode):
	transition_lines.append([from_state, to_state])


func _create_simple_state_machine():
	var sampler_a = _create_and_add_state("BLTAnimationNodeSampler", "Sampler A")
	var sampler_b = _create_and_add_state("BLTAnimationNodeSampler", "Sampler B")
	var sampler_c = _create_and_add_state("BLTAnimationNodeSampler", "Sampler C")


func _add_simple_state_machine_transitions():
	var sampler_a = state_node_to_graph_node[state_machine.get_state("Sampler A")]
	var sampler_b = state_node_to_graph_node[state_machine.get_state("Sampler B")]
	var sampler_c = state_node_to_graph_node[state_machine.get_state("Sampler C")]
	
	state_machine_graph_edit.add_transition(sampler_a, sampler_b)
	state_machine_graph_edit.add_transition(sampler_a, sampler_c)
	state_machine_graph_edit.add_transition(sampler_b, sampler_c)
	
	state_machine_graph_edit.queue_redraw()
