@tool
class_name BlendalotMainPanel
extends Control

@onready var blend_tree_graph_edit: GraphEdit = %BlendTreeGraphEdit
@onready var file_name_line_edit: LineEdit = %FileNameLineEdit
@onready var tree_option_button: OptionButton = %TreeOptionButton
@onready var add_node_popup_menu: PopupMenu = %AddNodePopupMenu

var root_animation_node:BLTAnimationNode = null
var last_edited_graph_node:GraphNode = null
var blend_tree:BLTAnimationNodeBlendTree = BLTAnimationNodeBlendTree.new()
var blend_tree_node_to_graph_node = {}
var graph_node_to_blend_tree_node = {}
var selected_nodes = {}
var new_node_position = Vector2.INF

var registered_nodes = [ 
	"BLTAnimationNodeSampler",
	"BLTAnimationNodeBlend2",
	"BLTAnimationNodeBlendTree",
	]

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	add_node_popup_menu.clear(true)
	
	for node_name in registered_nodes:
		add_node_popup_menu.add_item(node_name)


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass


func create_node_for_blt_node(blt_node: BLTAnimationNode) -> GraphNode:
	var result_graph_node:GraphNode = GraphNode.new()
	result_graph_node.name = blt_node.resource_name
	result_graph_node.title = blt_node.resource_name
	result_graph_node.position_offset = blt_node.graph_offset
	
	var result_slot_offset = 0
	
	if (blt_node.get_class() != "BLTAnimationNodeOutput"):
		result_slot_offset = 1
		var output_slot_label:Label = Label.new()
		output_slot_label.text = "Result"
		result_graph_node.add_child(output_slot_label)
		result_graph_node.set_slot(0, false, 1, Color.WHITE, true, 1, Color.WHITE)	
	
	var inputs = blt_node.get_input_names()
	for i in range(len(inputs)):
		var slot_label:Label = Label.new()
		slot_label.text = inputs[i]
		result_graph_node.add_child(slot_label)
		result_graph_node.set_slot(i + result_slot_offset, true, 1, Color.WHITE, false, 1, Color.BLACK)
	
	blt_node.node_changed.connect(_trigger_animation_graph_initialize)
	
	return result_graph_node


func _on_blend_tree_graph_edit_delete_nodes_request(nodes: Array[StringName]) -> void:
	for node_name:StringName in nodes:
		print("remove node '%s'" % node_name)
		var blend_tree_node:BLTAnimationNode = blend_tree.get_node(node_name)
		
		if blend_tree_node == null:
			push_error("Cannot delete node '%s': node not found." % node_name)
			continue
		
		if blend_tree_node == blend_tree.get_output_node():
			push_warning("Output node not allowed to be removed.")
			continue
		
		blend_tree_node.node_changed.disconnect(_trigger_animation_graph_initialize)
		
		var graph_node:GraphNode = blend_tree_node_to_graph_node[blend_tree_node]
		blend_tree.remove_node(blend_tree_node)
		blend_tree_node_to_graph_node.erase(blend_tree_node)
		
		_blend_tree_graph_edit_remove_node_connections(graph_node)
		graph_node_to_blend_tree_node.erase(graph_node)
		blend_tree_graph_edit.remove_child(graph_node)
		_on_blend_tree_graph_edit_node_deselected(graph_node)
		
		EditorInterface.get_inspector().edit(null)


func _on_reset_graph_button_pressed() -> void:
	EditorInterface.get_inspector().edit(null)

	_reset_editor()
	_update_editor_from_blend_tree()
	
	var graph_rect:Rect2 = blend_tree_graph_edit.get_rect()
	blend_tree_graph_edit.scroll_offset = graph_rect.size * -0.5 - Vector2(200,0)


func _reset_editor():
	for child in blend_tree_graph_edit.get_children():
		if child.name == "_connection_layer":
			continue
			
		child.get_parent().remove_child(child)
		child.queue_free()
	
	blend_tree_graph_edit.clear_connections()
	
	blend_tree = BLTAnimationNodeBlendTree.new()
	blend_tree_node_to_graph_node = {}
	graph_node_to_blend_tree_node = {}
	selected_nodes = {}


func edit_blend_tree(blend_tree_animation_node:BLTAnimationNode):
	print("Starting to edit blend_tree_animation_node " + str(blend_tree_animation_node))
	print("Owner: %s" % blend_tree_animation_node)
	_reset_editor()
	root_animation_node = blend_tree_animation_node
	blend_tree = blend_tree_animation_node
	
	_update_editor_nodes_from_blend_tree()
	_update_editor_connections_from_blend_tree()


func _update_editor_nodes_from_blend_tree():
	for node_name in blend_tree.get_node_names():
		var blend_tree_node:BLTAnimationNode = blend_tree.get_node(node_name)
		var graph_node:GraphNode = create_node_for_blt_node(blend_tree_node)
		blend_tree_graph_edit.add_child(graph_node)
		
		blend_tree_node_to_graph_node[blend_tree_node] = graph_node
		graph_node_to_blend_tree_node[graph_node] = blend_tree_node


func _update_editor_connections_from_blend_tree():
	var connection_array = blend_tree.get_connections()

	for i in range(len(connection_array) / 3):
		var source_node:BLTAnimationNode = connection_array[i * 3]
		var target_node:BLTAnimationNode = connection_array[i * 3 + 1]
		var target_port = connection_array[i * 3 + 2]
		
		var source_graph_node = blend_tree_node_to_graph_node[source_node]
		
		var connect_result = blend_tree_graph_edit.connect_node(source_node.resource_name, 0, target_node.resource_name, target_node.get_input_index(target_port), true)


func _update_editor_from_blend_tree():
	_update_editor_nodes_from_blend_tree()
	_update_editor_connections_from_blend_tree()


func _on_save_button_pressed() -> void:
	ResourceSaver.save(blend_tree, "res://" + file_name_line_edit.text)


func _on_load_button_pressed() -> void:
	var loaded_blend_tree:BLTAnimationNodeBlendTree = ResourceLoader.load("res://" + file_name_line_edit.text, "BLTAnimationNodeBlendTree", 0)

	if loaded_blend_tree:
		_reset_editor()
		blend_tree = loaded_blend_tree
		_update_editor_from_blend_tree()


func _on_instantiate_tree_button_pressed() -> void:
	var graph_name = tree_option_button.get_item_text(tree_option_button.selected)
	
	if graph_name == "AnimationSampler":
		_reset_editor()
		
		var sampler_node:BLTAnimationNodeSampler = BLTAnimationNodeSampler.new()
		sampler_node.animation = "SampleLibrary/Idle"
		
		blend_tree.add_node(sampler_node)
		blend_tree.add_connection(sampler_node, blend_tree.get_output_node(), "Output")
		
		_update_editor_from_blend_tree()
	elif graph_name == "Blend2":
		_reset_editor()
		
		var sampler_node_walk:BLTAnimationNodeSampler = BLTAnimationNodeSampler.new()
		sampler_node_walk.animation = "SampleLibrary/Walk"

		var sampler_node_run:BLTAnimationNodeSampler = BLTAnimationNodeSampler.new()
		sampler_node_run.animation = "SampleLibrary/Run"
		
		var blend2_node:BLTAnimationNodeBlend2 = BLTAnimationNodeBlend2.new()
				
		blend_tree.add_node(sampler_node_walk)
		blend_tree.add_node(sampler_node_run)
		blend_tree.add_node(blend2_node)
		
		blend_tree.add_connection(blend2_node, blend_tree.get_output_node(), "Output")
		
		blend_tree.add_connection(sampler_node_walk, blend2_node, "Input0")
		blend_tree.add_connection(sampler_node_run, blend2_node, "Input1")
		
		_update_editor_from_blend_tree()


func _on_blend_tree_graph_edit_connection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	print("Trying to connect '%s' port %d to node '%s' port %d" % [from_node, from_port, to_node, to_port])
	
	var source_node:BLTAnimationNode = blend_tree.get_node(from_node)
	var target_node:BLTAnimationNode = blend_tree.get_node(to_node)
	
	if target_node == null:
		push_error("Invalid connection, target node %s not found." % to_node)
		return
	
	var target_node_port_name = target_node.get_input_names()[to_port]
	
	var connection_result = blend_tree.is_connection_valid(source_node, target_node, target_node_port_name)
	if connection_result != blend_tree.CONNECTION_OK:
		push_error("Could not add connection (error %d)" % connection_result)
		return
	
	blend_tree.add_connection(source_node, target_node, target_node_port_name)
	
	var connect_result = blend_tree_graph_edit.connect_node(from_node, from_port, to_node, to_port, true)
	print("graph connect result: " + str(connect_result))
	
	print("Success!")


func _on_blend_tree_graph_edit_end_node_move() -> void:
	for graph_node:GraphNode in selected_nodes.keys():
		graph_node_to_blend_tree_node[graph_node].graph_offset = graph_node.position_offset


func _on_blend_tree_graph_edit_begin_node_move() -> void:
	pass # Replace with function body.


func _on_blend_tree_graph_edit_node_selected(graph_node: Node) -> void:
	selected_nodes[graph_node] = graph_node
	last_edited_graph_node = graph_node
	EditorInterface.get_inspector().edit(graph_node_to_blend_tree_node[graph_node])


func _on_blend_tree_graph_edit_node_deselected(graph_node: Node) -> void:
	if selected_nodes.has(graph_node):
		selected_nodes.erase(graph_node)


func _on_blend_tree_graph_edit_popup_request(at_position: Vector2) -> void:
	add_node_popup_menu.position = get_screen_position() + get_local_mouse_position()
	add_node_popup_menu.reset_size()
	add_node_popup_menu.popup()
	new_node_position = blend_tree_graph_edit.scroll_offset + at_position


func _on_add_node_popup_menu_index_pressed(index: int) -> void:
	var new_blend_tree_node: BLTAnimationNode = ClassDB.instantiate(registered_nodes[index])
	blend_tree.add_node(new_blend_tree_node)	
	
	var graph_node:GraphNode = create_node_for_blt_node(new_blend_tree_node)
	blend_tree_graph_edit.add_child(graph_node)
	
	graph_node_to_blend_tree_node[graph_node] = new_blend_tree_node
	blend_tree_node_to_graph_node[new_blend_tree_node] = graph_node
	
	if new_node_position != Vector2.INF:
		graph_node.position_offset = new_node_position
		new_blend_tree_node.graph_offset = new_node_position
	
	new_node_position = Vector2.INF


func _blend_tree_graph_edit_remove_node_connections(graph_node:GraphNode):
	var node_connections:Array = []
	
	for connection:Dictionary in blend_tree_graph_edit.connections:
		if connection["from_node"] == graph_node.name or connection["to_node"] == graph_node.name:
			node_connections.append(connection)
	
	for node_connection:Dictionary in node_connections:
		print("Removing connection %s" % str(node_connection))
		blend_tree_graph_edit.disconnect_node(node_connection["from_node"], node_connection["from_port"], node_connection["to_node"], node_connection["to_port"])


func _on_blend_tree_graph_edit_disconnection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	print("removing connection")
	
	var blend_tree_source_node = blend_tree.get_node(from_node)
	var blend_tree_target_node = blend_tree.get_node(to_node)
	var target_port_name = blend_tree_target_node.get_input_names()[to_port]
	blend_tree.remove_connection(blend_tree_source_node, blend_tree_target_node, target_port_name)
	
	blend_tree_graph_edit.disconnect_node(from_node, from_port, to_node, to_port)


func _trigger_animation_graph_initialize(node_name:StringName) -> void:
	root_animation_node.node_changed.emit("root")	


func _on_visibility_changed() -> void:
	if visible and is_instance_valid(last_edited_graph_node):
		_on_blend_tree_graph_edit_node_selected(last_edited_graph_node)
