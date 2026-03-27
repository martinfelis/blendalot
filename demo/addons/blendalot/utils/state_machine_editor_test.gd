extends Control
class_name StateMachineEditorTest

@onready var state_machine_graph_edit: StateMachineGraphEdit = %StateMachineGraphEdit

var iter:int = 0

func _on_add_state_button_pressed() -> void:
	var state_node:StateMachineState = preload("res://addons/blendalot/state_machine_state.tscn").instantiate()
	state_machine_graph_edit.add_child(state_node)
	state_node.title = "blaa " + str(iter)
	state_node.transition_border_size = 10
	state_node.active = false
	iter = iter + 1
