@tool

extends GraphElement
class_name StateMachineState

signal transition_drag_start
signal transition_drag_end

const TRANSITION_PANEL_COLOR_DEFAULT:Color = Color.WEB_GRAY
const TRANSITION_PANEL_COLOR_HOVER:Color = Color.CRIMSON
const STATE_PANEL_COLOR_DEFAULT:Color = Color.DIM_GRAY
const STATE_PANEL_COLOR_HOVER:Color = Color.WEB_GREEN

@onready var transition_drag_container: MarginContainer = %TransitionDragContainer
@onready var title_label: Label = %TitleLabel
@onready var transition_drag_panel: Panel = %TransitionDragPanel
@onready var selection_outline_panel: Panel = %SelectionOutlinePanel
@onready var state_content_panel: Panel = %StateContentPanel

@export var transition_border_size:float = 10 :
	set(value):
		if not is_instance_valid(transition_drag_container):
			await ready
		
		transition_border_size = value
		transition_drag_container.add_theme_constant_override("margin_top", transition_border_size)
		transition_drag_container.add_theme_constant_override("margin_left", transition_border_size)
		transition_drag_container.add_theme_constant_override("margin_bottom", transition_border_size)
		transition_drag_container.add_theme_constant_override("margin_right", transition_border_size)
	get():
		return transition_border_size

@export var transition_border_color:Color = Color.GRAY :
	set(value):
		transition_border_color = value
		
		if not is_instance_valid(transition_drag_container):
			await ready
		
		var stylebox = transition_drag_panel.get_theme_stylebox("panel").duplicate()
		stylebox.set("bg_color", value)
		transition_drag_panel.add_theme_stylebox_override("panel", stylebox)
		
		queue_redraw()
	get():
		return transition_border_color

@export var state_content_color:Color = Color.GRAY :
	set(value):
		state_content_color = value
		
		if not is_instance_valid(transition_drag_container):
			await ready
		
		var stylebox = state_content_panel.get_theme_stylebox("panel").duplicate()
		stylebox.set("bg_color", value)
		state_content_panel.add_theme_stylebox_override("panel", stylebox)
		queue_redraw()
	get():
		return state_content_color

@export var title:String = "State" :
	set(value):
		title = value
		if not is_instance_valid(title_label):
			await ready
		title_label.text = value
	get():
		if not is_instance_valid(title_label):
			await ready
		return title_label.text

@export var active:bool = false:
	set(value):
		active = value
		if not is_instance_valid(selection_outline_panel):
			await ready
		selection_outline_panel.visible = value
	get():
		return active


var is_dragging_node = false
var is_mouse_over_transition_border = false
var is_dragging_transition = false
var is_mouse_hovering = false

func _ready() -> void:
	var parent_state_machine_graph_edit:StateMachineGraphEdit = get_parent() as StateMachineGraphEdit
	if not is_instance_valid(parent_state_machine_graph_edit):
		if not Engine.is_editor_hint():
			push_warning("StateMachineState %s (path: %s) not a child of a StateMachineGraphEdit (editor hint %s). Transition editing limited." % [self.title, self.get_path()])
		return
	
	title_label.text = title
	
	transition_drag_start.connect(parent_state_machine_graph_edit.on_transition_drag_start)
	transition_drag_end.connect(parent_state_machine_graph_edit.on_transition_drag_end)
	mouse_entered.connect(parent_state_machine_graph_edit.on_state_mouse_entered.bind(self))
	mouse_exited.connect(parent_state_machine_graph_edit.on_state_mouse_exited.bind(self))


func _process(delta: float) -> void:
	if not is_mouse_hovering and not is_mouse_over_transition_border:
		return
	
	if is_mouse_hovering and not state_content_panel.get_global_rect().has_point(get_global_mouse_position()):
		is_mouse_hovering = false
		mouse_exited.emit()
	
	if is_mouse_over_transition_border and not transition_drag_container.get_global_rect().has_point(get_global_mouse_position()):
		is_mouse_over_transition_border = false
	
	if not get_global_rect().has_point(get_global_mouse_position()):
		is_mouse_over_transition_border = false
		is_mouse_hovering = false
	
	_update_state_colors()
	

func _update_state_colors() -> void:
	if is_mouse_hovering:
		state_content_color = STATE_PANEL_COLOR_HOVER
	else:
		state_content_color = STATE_PANEL_COLOR_DEFAULT
	
	if is_mouse_over_transition_border:
		transition_border_color = TRANSITION_PANEL_COLOR_HOVER
	else:
		transition_border_color = TRANSITION_PANEL_COLOR_DEFAULT


func _update_mouse_over_state() -> void:
	if is_dragging_node:
		return
	
	if not get_global_rect().has_point(get_global_mouse_position()):
		is_mouse_over_transition_border = false
		is_mouse_hovering = false
	
	if state_content_panel.get_global_rect().has_point(state_content_panel.get_global_mouse_position()):
		is_mouse_over_transition_border = false
		is_mouse_hovering = true
		selectable=true
	else:
		is_mouse_over_transition_border = true
		is_mouse_hovering = false
		selectable=false
	
	_update_state_colors()


func _on_gui_input(event: InputEvent) -> void:
	# print("event %s\n    over_transition_border %s selectable %s is_dragging_node %s is_dragging_transition %s" % [event, is_mouse_over_transition_border, selectable, is_dragging_node, is_dragging_transition] )
	
	var mouse_motion_event:InputEventMouseMotion = event as InputEventMouseMotion
	if mouse_motion_event:
		_update_mouse_over_state()
		
		# We ignore mouse motion events that are outside of our rect.
		if not get_rect().has_point(get_local_mouse_position()):
			return
		StateMachineState
		if not is_mouse_hovering:
			mouse_entered.emit.call_deferred(self)
		
		if is_dragging_node:
			return
		
		return
	
	var mouse_button_event:InputEventMouseButton = event as InputEventMouseButton
	if mouse_button_event:
		_update_mouse_over_state()
		
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

func get_content() -> Control:
	var editor_node_content:Control = find_child("StateContentContainer", true, false) as Control
	
	if not is_instance_valid(editor_node_content):
		push_error("Cannot find state content control for state %s" % self)
	
	return editor_node_content
