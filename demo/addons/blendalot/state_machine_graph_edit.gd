@tool

extends GraphEdit
class_name StateMachineGraphEdit

var transitions = []
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


func _process(_delta: float) -> void:
	if not is_instance_valid(transition_drag_start_state):
		return
	
	queue_redraw()


func _draw_transition_line(start_position:Vector2, end_position:Vector2) -> void:
	var line_center = (start_position + end_position) * 0.5
	var direction = (end_position - start_position).normalized()
	var orthogonal = Vector2(-direction.y, direction.x)
	
	draw_line(start_position, end_position, Color.WHITE, 2.0, true)
	# draw_circle(line_center, 4.0, Color.GREEN)
	
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
		
		_draw_transition_line(get_state_graph_position(from_node), get_state_graph_position(to_node))
	
	if is_instance_valid(transition_drag_start_state):
		var target_position:Vector2
		if hovered_state:
			target_position = get_state_graph_position(hovered_state)
		else:
			target_position = get_mouse_graph_position() * zoom - scroll_offset * zoom
		_draw_transition_line(get_state_graph_position(transition_drag_start_state), get_mouse_graph_position() * zoom - scroll_offset * zoom)


func on_transition_drag_start(state_graph_element:GraphElement):
	print("Starting transition start from state %s" % state_graph_element)
	transition_drag_start_state = state_graph_element


func on_transition_drag_end(position:Vector2):
	print("Ending transition at pos %s" % position)
	transition_drag_start_state = null


func on_state_mouse_entered(state:StateMachineState):
	print("mouse entered %s" % state.title)
	hovered_state = state


func on_state_mouse_exited(state:StateMachineState):
	print("mouse exited %s" % state.title)
	hovered_state = null
