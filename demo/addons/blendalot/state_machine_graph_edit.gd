@tool

extends GraphEdit
class_name StateMachineGraphEdit

signal transition_add_request(from_state: StringName, to_state: StringName)

# TODO
signal transition_selected(from_state: StringName, to_state: StringName)
signal transition_deselected(from_state: StringName, to_state: StringName)
signal transition_remove_request(from_state: StringName, to_state: StringName)
signal begin_state_move()
signal end_state_move()

# Postponed
# signal state_begin_move

enum TransitionDrawMode {
	DEFAULT,
	HOVER,
	SELECTED
}

const TRANSITION_HOVER_DISTANCE:float = 20
const TRANSITION_LINE_COLOR_DEFAULT:Color = Color.WHITE
const TRANSITION_LINE_WIDTH_DEFAULT:float = 2
const TRANSITION_LINE_COLOR_HOVER:Color = Color.PALE_GOLDENROD
const TRANSITION_LINE_WIDTH_HOVER:float = 10
const TRANSITION_LINE_COLOR_SELECTED:Color = Color.WHITE
const TRANSITION_LINE_WIDTH_SELECTED:float = 6

@onready var debug_label: Label = %DebugLabel

var transitions = []
var closest_transition_to_mouse = null
var selected_transition = null
var transition_drag_start_state:GraphElement = null
var hovered_state:GraphElement = null


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	transitions = []


func add_transition(from_node:GraphElement, to_node:GraphElement):
	transitions.append([from_node, to_node])


func get_mouse_graph_position() -> Vector2:
	var local_mouse:Vector2 = get_local_mouse_position()
	return local_mouse / zoom + scroll_offset


func get_state_graph_position(state:GraphElement) -> Vector2:
	return (state.position_offset) * zoom + state.get_rect().size * 0.5 - scroll_offset


func calc_distance_point_to_line_segment(p:Vector2, a:Vector2, b:Vector2) -> float:
	var dist_a_b_squared = (b - a).length_squared()
	var t:float = 0
	
	if is_zero_approx(dist_a_b_squared):
		t = 0.5
	else:
		t = max(0, min(1, (p - a).dot(b - a) / dist_a_b_squared))
	
	return (p - (a + t * (b - a))).length()


func _update_closest_transition_to_mouse() -> void:
	var closest_distance:float = INF
	for transition in transitions:
		var distance:float = calc_distance_point_to_line_segment(
			get_mouse_graph_position() * zoom - scroll_offset * zoom, 
			get_state_graph_position(transition[0]),
			get_state_graph_position(transition[1]))
		if distance < closest_distance:
			closest_transition_to_mouse = transition
			closest_distance = distance
	
	if closest_distance > TRANSITION_HOVER_DISTANCE:
		closest_distance = INF
		closest_transition_to_mouse = null
	
	if closest_transition_to_mouse != null:
		debug_label.text = "Closest transition: %s -> %s (%s)" % [closest_transition_to_mouse[0], closest_transition_to_mouse[1], closest_distance]
	else:
		debug_label.text = "No transition found: %s" % closest_distance


func _process(_delta: float) -> void:
	_update_closest_transition_to_mouse()
	queue_redraw()
	
	if not is_instance_valid(transition_drag_start_state):
		return
	
	queue_redraw()


func _draw_transition_line(start_position:Vector2, end_position:Vector2, draw_mode:TransitionDrawMode = TransitionDrawMode.DEFAULT) -> void:
	var line_width:float = TRANSITION_LINE_WIDTH_DEFAULT
	var line_color:Color = TRANSITION_LINE_COLOR_DEFAULT
	
	if draw_mode == TransitionDrawMode.HOVER:
		line_width = TRANSITION_LINE_WIDTH_HOVER
		line_color = TRANSITION_LINE_COLOR_HOVER
	elif draw_mode == TransitionDrawMode.SELECTED:
		line_width = TRANSITION_LINE_WIDTH_SELECTED
		line_color = TRANSITION_LINE_COLOR_SELECTED

	var line_center = (start_position + end_position) * 0.5
	var direction = (end_position - start_position).normalized()
	var orthogonal = Vector2(-direction.y, direction.x)
	
	draw_line(start_position, end_position, line_color, line_width, true)
	
	var triangle_size = 20
	
	var triangle_vertices:PackedVector2Array = PackedVector2Array([
		line_center - direction * triangle_size * 0.5 + orthogonal * triangle_size * 0.5,
		line_center + direction * triangle_size * 0.5,
		line_center - direction * triangle_size * 0.5 - orthogonal * triangle_size * 0.5,
		line_center - direction * triangle_size * 0.5 + orthogonal * triangle_size * 0.5,
		])

	var triangle_color:Color = Color.GREEN_YELLOW
	var triangle_colors:PackedColorArray = PackedColorArray([
		triangle_color, triangle_color,
		triangle_color, triangle_color
	])
	
	var triangle_outline_width = 2
	var triangle_outline_color = Color.WEB_GREEN

	draw_polygon(triangle_vertices, triangle_colors)
	draw_polyline(triangle_vertices, triangle_outline_color, 1, true)


func _draw():
	for transition in transitions:
		var from_node:GraphElement = transition[0]
		var to_node:GraphElement = transition[1]
		
		var start_position:Vector2 = (from_node.position_offset) * zoom + from_node.get_rect().size * 0.5 - scroll_offset
		var end_position:Vector2 = (to_node.position_offset) * zoom + to_node.get_rect().size * 0.5 - scroll_offset
		
		var transition_draw_mode:TransitionDrawMode = TransitionDrawMode.DEFAULT
		if closest_transition_to_mouse != null and closest_transition_to_mouse[0] == from_node and closest_transition_to_mouse[1] == to_node:
			transition_draw_mode = TransitionDrawMode.HOVER
		elif selected_transition != null and selected_transition[0] == from_node and selected_transition[1] == to_node:
			transition_draw_mode = TransitionDrawMode.SELECTED
		
		_draw_transition_line(get_state_graph_position(from_node), get_state_graph_position(to_node), transition_draw_mode)
	
	if is_instance_valid(transition_drag_start_state):
		var target_position:Vector2
		if hovered_state:
			target_position = get_state_graph_position(hovered_state)
		else:
			target_position = get_mouse_graph_position() * zoom - scroll_offset * zoom
		_draw_transition_line(get_state_graph_position(transition_drag_start_state), target_position)


func on_transition_drag_start(state_graph_element:GraphElement):
	transition_drag_start_state = state_graph_element


func on_transition_drag_end(position:Vector2):
	if hovered_state != null:
		transition_add_request.emit(transition_drag_start_state.title, hovered_state.title)
	transition_drag_start_state = null


func on_state_mouse_entered(state:StateMachineState):
	hovered_state = state


func on_state_mouse_exited(state:StateMachineState):
	hovered_state = null


func _unhandled_input(event: InputEvent) -> void:
	var mouse_button_event:InputEventMouseButton = event as InputEventMouseButton
	if mouse_button_event and mouse_button_event.button_index == MOUSE_BUTTON_LEFT and mouse_button_event.pressed:
		if selected_transition != null and closest_transition_to_mouse != selected_transition:
			transition_deselected.emit(selected_transition[0].title, selected_transition[1].title)
			selected_transition = null
		
		if selected_transition == null and closest_transition_to_mouse != null:
			selected_transition = closest_transition_to_mouse
			transition_selected.emit(selected_transition[0].title, selected_transition[1].title)
