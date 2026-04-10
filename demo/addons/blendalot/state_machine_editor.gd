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
		var state_node:BLTAnimationNode = state_machine.get_state(state_name)
		var graph_node:GraphElement = create_graph_node_for_blt_node(state_node)
		state_machine_graph_edit.add_child(graph_node)
		
		state_node_to_graph_node[state_node] = graph_node
		graph_node_to_state_node[graph_node] = state_node


func create_graph_node_for_blt_node(blt_node: BLTAnimationNode) -> StateMachineState:
	var state_graph_element:StateMachineState = preload("res://addons/blendalot/state_machine_state.tscn").instantiate()
	state_graph_element.name = blt_node.resource_name
	state_graph_element.title = blt_node.resource_name
	state_graph_element.transition_border_size = 10
	state_graph_element.position_offset = blt_node.position
	
	blt_node.node_changed.connect(_trigger_graph_changed)
	
	if blt_node.get_class() == "BLTBlendTree" or blt_node.get_class() == "BLTStateMachine":
		state_graph_element.gui_input.connect(_on_node_gui_input.bind(state_graph_element))
	
	return state_graph_element


func _update_editor_connections_from_state_machine():
	var transitions_array = state_machine.get_transitions()

	var success:bool = true
	for i in range(len(transitions_array) / 3):
		var from_state:BLTAnimationNode = transitions_array[i * 3]
		var to_state:BLTAnimationNode = transitions_array[i * 3 + 1]
		var transition:BLTStateMachineTransition = transitions_array[i * 3 + 2]
		
		var from_state_node = state_node_to_graph_node[from_state]
		
		var connect_result = state_machine_graph_edit.add_transition(from_state.resource_name, to_state.resource_name)
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
func _on_state_machine_graph_edit_transition_add_request(from_state: StringName, to_state: StringName) -> void:
	var from_node:BLTAnimationNode = state_machine.get_state(from_state)
	var to_node:BLTAnimationNode = state_machine.get_state(to_state)

	if from_node == null:
		push_error("Invalid transition: from state %s not found." % to_state)
		return
	
	if to_node == null:
		push_error("Invalid transition: target state %s not found." % to_state)
		return
	
	var transition_result = state_machine.is_transition_valid(from_node, to_node)
	if transition_result != state_machine.TRANSITION_OK:
		push_error("Could not add transition (error %d)" % transition_result)
		return
	
	var transition:BLTStateMachineTransition = BLTStateMachineTransition.new()
	state_machine.add_transition(from_node, to_node, transition)
	state_machine_graph_edit.add_transition(from_state, to_state)


func _on_state_machine_graph_edit_transition_remove_request(from_state: StringName, to_state: StringName) -> void:
	var from_node:BLTAnimationNode = state_machine.get_state(from_state)
	var to_node:BLTAnimationNode = state_machine.get_state(to_state)

	if from_node == null:
		push_error("Cannot remove transition: from state %s not found." % to_state)
		return
	
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
		var state_machine_node:BLTAnimationNode = state_machine.get_state(node_name)
		
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
	
	var graph_node:StateMachineState = create_graph_node_for_blt_node(new_state_node)
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
		transition_trag_start_position = Vector2.INF


#
# Handle Node double click
#
func _on_node_gui_input(input_event:InputEvent, graph_node:GraphElement):
	# print("Got input event on graph node %s!" % graph_node.name)
	
	var mouse_button_event:InputEventMouseButton = input_event as InputEventMouseButton
	if mouse_button_event and mouse_button_event.double_click:
		get_viewport().set_input_as_handled()
		_on_node_double_click(graph_node)
		
		return


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


func _on_state_machine_graph_edit_transition_selected(from_state: StringName, to_state: StringName) -> void:
	pass # Replace with function body.


func _on_state_machine_graph_edit_transition_deselected(from_state: StringName, to_state: StringName) -> void:
	pass # Replace with function body.
