@tool

extends GraphEdit
class_name StateMachineGraphEdit

var transitions = []

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	transitions = []

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass

func add_transition(from_node:GraphNode, to_node:GraphNode):
	transitions.append([from_node, to_node])

func _draw():
	for transition in transitions:
		var from_node:GraphElement = transition[0]
		var to_node:GraphElement = transition[1]
		
		var start_position:Vector2 = (from_node.position_offset) * zoom + from_node.get_rect().size * 0.5 - scroll_offset
		var end_position:Vector2 = (to_node.position_offset) * zoom + to_node.get_rect().size * 0.5 - scroll_offset
		
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
