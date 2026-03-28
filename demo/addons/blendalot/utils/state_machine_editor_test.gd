extends Control
class_name StateMachineEditorTest

@onready var state_machine_graph_edit: StateMachineGraphEdit = %StateMachineGraphEdit

var iter:int = 0

var states = {}

func _ready():
	state_machine_graph_edit.transition_add_request.connect(_on_transition_add_request)

func _on_add_state_button_pressed() -> void:
	var state_node:StateMachineState = preload("res://addons/blendalot/state_machine_state.tscn").instantiate()
	state_machine_graph_edit.add_child(state_node)
	state_node.title = "State " + str(iter)
	state_node.transition_border_size = 10
	state_node.active = false
	iter = iter + 1
	states[state_node.title] = state_node

func _on_transition_add_request(from_state_name:String, to_state_name:String):
	state_machine_graph_edit.add_transition(states[from_state_name], states[to_state_name])
