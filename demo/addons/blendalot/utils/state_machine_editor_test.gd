extends Control
class_name StateMachineEditorTest

@onready var state_machine_graph_edit: StateMachineGraphEdit = %StateMachineGraphEdit

var iter:int = 0

var states = {}

func _ready():
	state_machine_graph_edit.transition_add_request.connect(_on_transition_add_request)
	state_machine_graph_edit.transition_remove_request.connect(_on_transition_remove_request)
	state_machine_graph_edit.transition_selected.connect(_on_transition_selected)
	state_machine_graph_edit.transition_deselected.connect(_on_transition_deselected)


func remove_state_transitions(node:GraphElement) -> void:
	var state_transition_indices:Array[int] = []
	
	for i in range(0, len(state_machine_graph_edit.transition_lines)):
		if state_machine_graph_edit.transition_lines[i].from_state == node or state_machine_graph_edit.transition_lines[i].to_state == node:
			state_transition_indices.append(i)

	if len(state_transition_indices) > 0:
		state_machine_graph_edit.queue_redraw()
		
	state_transition_indices.reverse()
	for index in state_transition_indices:
		state_machine_graph_edit.transition_lines.remove_at(index)

func _on_add_state_button_pressed() -> void:
	var state_node:StateMachineState = preload("res://addons/blendalot/state_machine_state.tscn").instantiate()
	state_machine_graph_edit.add_child(state_node)
	state_node.title = "State " + str(iter)
	state_node.transition_border_size = 10
	state_node.active = false
	state_node.position_offset = state_machine_graph_edit.get_rect().get_center() + state_machine_graph_edit.scroll_offset
	iter = iter + 1
	states[state_node.title] = state_node
	print("added state '%s'" % state_node)


func _on_transition_add_request(from_state_name:String, to_state_name:String):
	state_machine_graph_edit.add_transition(states[from_state_name], states[to_state_name])


func _on_transition_remove_request(from_state_name:String, to_state_name:String):
	print("will try to remove transition %s -> %s" % [from_state_name, to_state_name])
	state_machine_graph_edit.remove_transition(states[from_state_name], states[to_state_name])


func _on_transition_selected(from_state_name:String, to_state_name:String):
	print("selected transition from %s to %s" % [from_state_name, to_state_name])


func _on_transition_deselected(from_state_name:String, to_state_name:String):
	print("deselected transition from %s to %s" % [from_state_name, to_state_name])


func _on_state_machine_graph_edit_delete_nodes_request(nodes: Array[StringName]) -> void:
	for state_name:StringName in nodes:
		var state_node:StateMachineState = state_machine_graph_edit.find_child(state_name, false, false)
		if not is_instance_valid(state_node):
			push_warning("Cannot delete state '%s': StateMachineState not found!" % state_name)
			continue
		
		remove_state_transitions(state_node)
		state_machine_graph_edit.remove_child(state_node)
		state_node.queue_free()
