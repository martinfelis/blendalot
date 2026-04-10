@tool

extends GraphEdit
class_name StateMachineGraphEdit

signal transition_add_request(from_state: StringName, to_state: StringName)
signal transition_remove_request(from_state: StringName, to_state: StringName)
signal transition_selected(from_state: StringName, to_state: StringName)
signal transition_deselected(from_state: StringName, to_state: StringName)

class TransitionLine:
	var from_state:StateMachineState = null
	var to_state:StateMachineState = null
	var from_position:Vector2 = Vector2.INF
	var to_position:Vector2 = Vector2.INF
	
	func _init(from:StateMachineState, to:StateMachineState) -> void:
		from_state = from
		to_state = to

enum TransitionDrawMode {
	DEFAULT,
	HOVER,
	SELECTED
}

const TRANSITION_HOVER_DISTANCE:float = 20
const TRANSITION_LINE_COLOR_DEFAULT:Color = Color.WHITE
const TRANSITION_LINE_WIDTH_DEFAULT:float = 2
const TRANSITION_LINE_COLOR_HOVER:Color = Color.WHITE
const TRANSITION_LINE_WIDTH_HOVER:float = 6
const TRANSITION_LINE_COLOR_SELECTED:Color = Color.PALE_GOLDENROD
const TRANSITION_LINE_WIDTH_SELECTED:float = 10
const TRANSITION_BIDIRECTIONAL_OFFSET:float = 20

var hover_transition:TransitionLine = null
var selected_transition:TransitionLine = null
var transition_drag_start_state:GraphElement = null
var hovered_state:GraphElement = null

var state_out_transitions = {}
var state_in_transitions = {}
var transition_lines = []

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	transition_lines = []
	state_out_transitions = {}
	state_in_transitions = {}


func add_transition(from_node_name:String, to_node_name:String) -> bool:
	var from_node:GraphElement = find_child(from_node_name, false, false) as GraphElement
	var to_node:GraphElement = find_child(to_node_name, false, false) as GraphElement
	
	assert(from_node != null)
	assert(to_node != null)
	
	if state_out_transitions.has(from_node):
		if state_out_transitions[from_node].find(to_node) != -1:
			return false
		state_out_transitions[from_node].append(to_node)
	else:
		state_out_transitions[from_node] = [to_node]
	
	if state_in_transitions.has(to_node):
		if state_in_transitions[to_node].find(from_node) != -1:
			return false
		state_in_transitions[to_node].append(from_node)
	else:
		state_in_transitions[to_node] = [from_node]
	
	transition_lines.append(TransitionLine.new(from_node, to_node))
	
	queue_redraw()
	
	return true


func remove_transition(from_node:GraphElement, to_node:GraphElement):
	if from_node == null or to_node == null:
		push_warning("Cannot remove transition %s -> %s. One node is null." % [from_node, to_node])
	
	for i in range(0, len(transition_lines)):
		if transition_lines[i].from_state == from_node and transition_lines[i].to_state == to_node:
			state_out_transitions[from_node].erase(to_node)
			state_in_transitions[to_node].erase(from_node)
			transition_lines.remove_at(i)

			assert(has_reverse_transition(to_node, from_node) == false)

			queue_redraw()
			return


func has_reverse_transition(from_node:GraphElement, to_node:GraphElement) -> bool:
	var transition_line:TransitionLine = null
	if state_out_transitions.has(to_node):
		return state_out_transitions[to_node].find(from_node) != -1
	
	return false


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


func _update_hover_transition() -> bool:
	var previouse_hover_transition:TransitionLine = hover_transition
	if hovered_state != null:
		hover_transition = null
		return previouse_hover_transition != null
	
	var closest_distance:float = INF
	for transition:TransitionLine in transition_lines:
		var distance:float = calc_distance_point_to_line_segment(
			get_mouse_graph_position() * zoom - scroll_offset * zoom, 
			transition.from_position,
			transition.to_position)
		if distance < closest_distance:
			hover_transition = transition
			closest_distance = distance
	
	if closest_distance > TRANSITION_HOVER_DISTANCE:
		closest_distance = INF
		hover_transition = null
	
	return previouse_hover_transition != hover_transition


func _process(_delta: float) -> void:
	if _update_hover_transition():
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
	var orthogonal = direction.orthogonal()
	
	draw_line(start_position, end_position, line_color, line_width * zoom, true)
	
	var triangle_size = 20 * zoom
	
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


func _update_transition_lines() -> void:
	for transition_line:TransitionLine in transition_lines:
		transition_line.from_position = get_state_graph_position(transition_line.from_state)
		transition_line.to_position = get_state_graph_position(transition_line.to_state)
		
		var line_offset:Vector2 = Vector2.ZERO
		
		if has_reverse_transition(transition_line.from_state, transition_line.to_state):
			line_offset = (transition_line.to_position - transition_line.from_position).normalized().orthogonal() * TRANSITION_BIDIRECTIONAL_OFFSET * zoom
			
		transition_line.from_position = get_state_graph_position(transition_line.from_state) + line_offset
		transition_line.to_position = get_state_graph_position(transition_line.to_state) + line_offset


func _draw():
	_update_transition_lines()
	
	for transition_line:TransitionLine in transition_lines:
		var from_node:GraphElement = transition_line.from_state
		var to_node:GraphElement = transition_line.to_state
		
		var transition_draw_mode:TransitionDrawMode = TransitionDrawMode.DEFAULT
		if hover_transition != null and hover_transition.from_state == from_node and hover_transition.to_state == to_node:
			transition_draw_mode = TransitionDrawMode.HOVER
		elif selected_transition != null and selected_transition.from_state == from_node and selected_transition.to_state == to_node:
			transition_draw_mode = TransitionDrawMode.SELECTED
		
		_draw_transition_line(transition_line.from_position, transition_line.to_position, transition_draw_mode)
	
		
	for transition:TransitionLine in transition_lines:
		var from_node:GraphElement = transition.from_state
		var to_node:GraphElement = transition.to_state
		
		var start_position:Vector2 = (from_node.position_offset) * zoom + from_node.get_rect().size * 0.5 - scroll_offset
		var end_position:Vector2 = (to_node.position_offset) * zoom + to_node.get_rect().size * 0.5 - scroll_offset
		

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


func _on_gui_input(event: InputEvent) -> void:
	var mouse_button_event:InputEventMouseButton = event as InputEventMouseButton
	if mouse_button_event and mouse_button_event.button_index == MOUSE_BUTTON_LEFT and mouse_button_event.pressed:
		if selected_transition != null and hover_transition != selected_transition:
			transition_deselected.emit(selected_transition.from_state.title, selected_transition.to_state.title)
			selected_transition = null
			queue_redraw()
			get_viewport().set_input_as_handled()
		
		if selected_transition == null and hover_transition != null:
			selected_transition = hover_transition
			transition_selected.emit(selected_transition.from_state.title, selected_transition.to_state.title)
			queue_redraw()
			get_viewport().set_input_as_handled()
		
		return

	var key_event:InputEventKey = event as InputEventKey
	if key_event and key_event.pressed and key_event.physical_keycode == KEY_DELETE:
		if selected_transition != null:
			print("trying to remove transition %s" % selected_transition)
			transition_remove_request.emit(selected_transition.from_state.title, selected_transition.to_state.title)
			get_viewport().set_input_as_handled()
			return
