@tool

extends Control
class_name BltStateMachineEditor

@onready var registered_nodes = load ("res://addons/blendalot/blendalot_node_registry.gd").new().registered_nodes

@onready var state_machine_graph_edit: StateMachineGraphEdit = %StateMachineGraphEdit
@onready var add_state_popup_menu: PopupMenu = %AddStatePopupMenu

signal edit_subgraph(blt_node:BLTAnimationNode)
signal graph_changed()

var state_machine:BLTStateMachine

var state_node_to_editor_node = {}
var editor_node_to_state_node = {}
var transition_lines = []

var selected_nodes = {}
var selected_transition:BLTStateMachineTransition = null
var last_selected_editor_node:GraphElement = null
var new_state_position:Vector2 = Vector2.ZERO
var transition_trag_start_position:Vector2 = Vector2.INF


func _ready() -> void:
	add_state_popup_menu.clear(true)
	
	for node_name in registered_nodes:
		# Only nodes without inputs can be used as states.
		var blt_node:BLTAnimationNode = ClassDB.instantiate(node_name)
		if blt_node.get_input_count() == 0:
			add_state_popup_menu.add_item(node_name)


func _reset_editor():
	for child in state_machine_graph_edit.get_children():
		if child.name == "_connection_layer":
			continue
			
		child.get_parent().remove_child(child)
		child.queue_free()
	
	if is_instance_valid(state_machine):
		if state_machine.has_connections("node_changed"):
			state_machine.disconnect("node_changed", _trigger_graph_changed)
		
		for state_node:BLTAnimationNode in state_node_to_editor_node.keys():
			state_node.changed.disconnect(_on_state_node_changed.bind(state_node))
	
		var transitions_array = state_machine.get_transitions()

		for i in range(len(transitions_array) / 3):
			var transition:BLTStateMachineTransition = transitions_array[i * 3 + 2]
			transition.changed.disconnect(_on_transition_changed.bind(transition))
	
	state_machine_graph_edit.reset()
	
	state_machine = null
	state_node_to_editor_node = {}
	editor_node_to_state_node = {}
	selected_nodes = {}


func edit_state_machine(blt_state_machine:BLTStateMachine):
	_reset_editor()
	
	state_machine = blt_state_machine
	state_machine_graph_edit.scroll_offset = state_machine.graph_offset
	
	_create_editor_nodes_from_state_machine()
	_create_editor_transitions_from_state_machine()

	state_machine.node_changed.connect(_trigger_graph_changed)


func _create_editor_nodes_from_state_machine():
	for state_name in state_machine.get_state_names():
		var state_node:BLTAnimationNode = state_machine.get_state(state_name)
		state_node.changed.connect(_on_state_node_changed.bind(state_node))
		var editor_node:GraphElement = create_editor_node_for_blt_node(state_node)
		state_machine_graph_edit.add_child(editor_node)
		
		state_node_to_editor_node[state_node] = editor_node
		editor_node_to_state_node[editor_node] = state_node


func create_editor_node_for_blt_node(blt_node: BLTAnimationNode) -> StateMachineState:
	var editor_node:StateMachineState = preload("res://addons/blendalot/state_machine_state.tscn").instantiate()
	editor_node.name = blt_node.resource_name
	editor_node.title = blt_node.resource_name
	editor_node.transition_border_size = 10
	editor_node.position_offset = blt_node.position
	var editor_node_content = editor_node.get_content()
	
	if not is_instance_valid(editor_node_content):
		return editor_node
	
	if blt_node.get_class() == "BLTBlendTree" or blt_node.get_class() == "BLTStateMachine":
		editor_node.gui_input.connect(_on_node_gui_input.bind(editor_node))
	
	if blt_node.get_class() == "BLTAnimationNodeSampler":
		var animation_sampler_node:BLTAnimationNodeSampler = blt_node as BLTAnimationNodeSampler
		var animation_selector_button = OptionButton.new()
		animation_selector_button.fit_to_longest_item = false
		editor_node_content.add_child(animation_selector_button)

		var animation_player:AnimationPlayer = animation_sampler_node.get_animation_player()
		
		if is_instance_valid(animation_player):
			animation_selector_button.item_selected.connect(_on_animation_select.bind(animation_sampler_node, animation_selector_button))
			
			for animation_name in animation_player.get_animation_list():
				animation_selector_button.add_item(animation_name)
				if animation_name == animation_sampler_node.animation:
					animation_selector_button.select(animation_selector_button.item_count - 1)
			
			# Select first animation by default.
			if animation_sampler_node.animation == "" and animation_selector_button.item_count > 0:
				# TODO: Need to manually trigger callable of the selection. Not sure why, though.
				animation_selector_button.select(0)
				_on_animation_select(0, animation_sampler_node, animation_selector_button)
	
	return editor_node


func _on_state_node_changed(state_node: BLTAnimationNode):
	# Handle renaming of Transitions
	# TODO (performance): only update affected node and transitions.
	var state_node_name = state_node.resource_name
	var current_state_machine = state_machine
	_reset_editor()
	edit_state_machine(current_state_machine)
	_trigger_graph_changed(state_node_name)


func _on_transition_changed(transition_node: BLTStateMachineTransition):
	var transition_node_name = transition_node.resource_name
	_trigger_graph_changed(transition_node_name)


func _trigger_graph_changed(_node_name):
	graph_changed.emit()


func _create_editor_transitions_from_state_machine():
	var transitions_array = state_machine.get_transitions()

	var success:bool = true
	for i in range(len(transitions_array) / 3):
		var from_state:BLTAnimationNode = transitions_array[i * 3]
		var to_state:BLTAnimationNode = transitions_array[i * 3 + 1]
		var transition:BLTStateMachineTransition = transitions_array[i * 3 + 2]

		transition.changed.connect(_on_transition_changed.bind(transition))
		
		var from_state_node = state_node_to_editor_node[from_state]
		
		var connect_result = state_machine_graph_edit.add_transition(from_state.resource_name, to_state.resource_name)
		if not connect_result:
			success = false


func _remove_graph_state_transitions(editor_node:GraphElement):
	var state_node:BLTAnimationNode = editor_node_to_state_node[editor_node]
	
	var transitions:Array = state_machine.get_transitions()
	for i in range(len(transitions) / 3):
		var from_state = transitions[i * 3]
		var to_state = transitions[i * 3 + 1]
		
		if from_state == state_node or to_state == state_node:		
			state_machine_graph_edit.remove_transition(state_node_to_editor_node[from_state], state_node_to_editor_node[to_state])


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
	
	if to_node == null:
		push_error("Cannot remove transition: target state %s not found." % to_state)
		return
	
	print("Removing transition %s -> %s" % [from_state, to_state])
	state_machine.remove_transition(from_node, to_node)
	state_machine_graph_edit.remove_transition(state_node_to_editor_node[from_node], state_node_to_editor_node[to_node])


func _on_state_machine_graph_edit_delete_nodes_request(nodes: Array[StringName]) -> void:
	for node_name:StringName in nodes:
		print("remove node '%s'" % node_name)
		var state_machine_node:BLTAnimationNode = state_machine.get_state(node_name)
		
		if state_machine_node == null:
			push_error("Cannot delete node '%s': node not found." % node_name)
			continue
		
		var editor_node:GraphElement = state_node_to_editor_node[state_machine_node]

		_on_state_machine_graph_edit_node_deselected(editor_node)
		_remove_graph_state_transitions(editor_node)
		
		editor_node_to_state_node.erase(editor_node)
		state_node_to_editor_node.erase(state_machine_node)

		state_machine_graph_edit.remove_child(editor_node)
		editor_node.queue_free()
		
		state_machine.remove_state(state_machine_node)
		
		EditorInterface.get_inspector().edit(null)


func _on_state_machine_graph_edit_end_node_move() -> void:
	for editor_node:GraphElement in selected_nodes.keys():
		editor_node_to_state_node[editor_node].position = editor_node.position_offset
	
	state_machine_graph_edit.queue_redraw()


func _on_state_machine_graph_edit_node_deselected(editor_node: Node) -> void:
	if selected_nodes.has(editor_node):
		selected_nodes.erase(editor_node)


func _on_state_machine_graph_edit_node_selected(editor_node: Node) -> void:
	selected_nodes[editor_node] = editor_node
	last_selected_editor_node = editor_node
	EditorInterface.get_inspector().edit(editor_node_to_state_node[editor_node])


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
	var new_state_node:BLTAnimationNode = ClassDB.instantiate(add_state_popup_menu.get_item_text(index))
	state_machine.add_state(new_state_node)

	var editor_node:StateMachineState = create_editor_node_for_blt_node(new_state_node)
	state_machine_graph_edit.add_child(editor_node)

	editor_node_to_state_node[editor_node] = new_state_node
	state_node_to_editor_node[new_state_node] = editor_node
	
	if new_state_position != Vector2.INF:
		editor_node.position_offset = new_state_position
		new_state_node.position = editor_node.position_offset
	
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
func _on_node_gui_input(input_event:InputEvent, editor_node:GraphElement):
	var mouse_button_event:InputEventMouseButton = input_event as InputEventMouseButton
	if mouse_button_event and mouse_button_event.double_click:
		get_viewport().set_input_as_handled()
		_on_node_double_click(editor_node)
		
		return


func _on_node_double_click(editor_node:GraphElement):
	var state_machine_node:BLTAnimationNode = editor_node_to_state_node[editor_node]
	
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


func _on_state_machine_graph_edit_transition_selected(from_state_name: StringName, to_state_name: StringName) -> void:
	var transitions:Array = state_machine.get_transitions()
	
	for i in range(0, len(transitions) / 3):
		var from_state:BLTAnimationNode = transitions[i * 3] as BLTAnimationNode
		var to_state:BLTAnimationNode = transitions[i * 3 + 1] as BLTAnimationNode
		
		if is_instance_valid(from_state) and is_instance_valid(to_state) and from_state.resource_name == from_state_name and to_state.resource_name == to_state_name:
			selected_transition = transitions[i * 3 + 2] as BLTStateMachineTransition
			EditorInterface.get_inspector().edit(selected_transition)
			return


func _on_state_machine_graph_edit_transition_deselected(from_state_name: StringName, to_state_name: StringName) -> void:
	var transitions:Array = state_machine.get_transitions()
	
	for i in range(0, len(transitions) / 3):
		var from_state:BLTAnimationNode = transitions[i * 3] as BLTAnimationNode
		var to_state:BLTAnimationNode = transitions[i * 3 + 1] as BLTAnimationNode
				
		if is_instance_valid(from_state) and is_instance_valid(to_state) and from_state.resource_name == from_state_name and to_state.resource_name == to_state_name:
			var transition = transitions[i * 3 + 2] as BLTStateMachineTransition
			if transition == selected_transition:
				selected_transition = null
				EditorInterface.get_inspector().edit(null)
				return
