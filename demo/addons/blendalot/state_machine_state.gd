@tool

extends GraphElement
class_name StateMachineState

signal transition_drag_start
signal transition_drag_end

@onready var transition_drag_container: MarginContainer = %TransitionDragContainer
@onready var title_label: Label = %TitleLabel
@onready var selection_outline_panel: Panel = %SelectionOutlinePanel
@onready var state_content_panel: Panel = %StateContentPanel

@export var transition_border_size:float = 10 :
	set(value):
		if not is_instance_valid(transition_drag_container):
			return
		
		transition_border_size = value
		transition_drag_container.add_theme_constant_override("margin_top", transition_border_size)
		transition_drag_container.add_theme_constant_override("margin_left", transition_border_size)
		transition_drag_container.add_theme_constant_override("margin_bottom", transition_border_size)
		transition_drag_container.add_theme_constant_override("margin_right", transition_border_size)
	get():
		return transition_border_size

@export var title:String = "State" :
	set(value):
		if not is_instance_valid(title_label):
			return
		title_label.text = value
		title = value
	get():
		if not is_instance_valid(title_label):
			return title
		return title_label.text

@export var active:bool = false:
	set(value):
		active = value
		if not is_instance_valid(selection_outline_panel):
			return
		selection_outline_panel.visible = value
	get():
		return active


var is_dragging_node = false
var is_mouse_over_transition_border = false
var is_dragging_transition = false

func _ready() -> void:
	var parent_state_machine_graph_edit:StateMachineGraphEdit = get_parent() as StateMachineGraphEdit
	if not is_instance_valid(parent_state_machine_graph_edit):
		push_warning("StateMachineState %s not a child of a StateMachineGraphEdit. Transition editing limited." % self.title)
		return
	
	transition_drag_start.connect(parent_state_machine_graph_edit.on_transition_drag_start)
	transition_drag_end.connect(parent_state_machine_graph_edit.on_transition_drag_end)
	mouse_entered.connect(parent_state_machine_graph_edit.on_state_mouse_entered)
	mouse_exited.connect(parent_state_machine_graph_edit.on_state_mouse_exited)


func _on_gui_input(event: InputEvent) -> void:
	# print("over_transition_border %s selectable %s is_dragging_node %s is_dragging_transition %s" % [is_mouse_over_transition_border, selectable, is_dragging_node, is_dragging_transition] )
	
	var mouse_motion_event:InputEventMouseMotion = event as InputEventMouseMotion
	if mouse_motion_event:
		if is_dragging_node:
			return
			
		if state_content_panel.get_rect().has_point(mouse_motion_event.position):
			is_mouse_over_transition_border = false
			selectable=true
		else:
			is_mouse_over_transition_border = true
			selectable=false
		
		return
	
	var mouse_button_event:InputEventMouseButton = event as InputEventMouseButton
	if mouse_button_event:
		if mouse_button_event.button_mask & MOUSE_BUTTON_LEFT:
			if mouse_button_event.pressed:
				if is_mouse_over_transition_border:
					transition_drag_start.emit(self)
					is_dragging_transition = true
				else:
					is_dragging_node = true
		else:
			is_dragging_node = false
			if is_dragging_transition:
				transition_drag_end.emit(mouse_button_event.position)
				is_dragging_transition = false
