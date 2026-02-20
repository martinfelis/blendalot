@tool

extends Control
class_name AnimationGraphEditor

@onready var breadcrumb_button_container: HBoxContainer = %BreadcrumbButtons
@onready var active_graph_control: Control = %ActiveGraphControl

var animation_graph:BLTAnimationGraph = null
var animation_graph_root_node:BLTAnimationNode = null
var graph_node_stack:Array[BLTAnimationNode] = []
var active_graph_edit:Control = null
var active_graph_edit_index = -1


func reset_graph_control():
	for child in active_graph_control.get_children():
		active_graph_control.remove_child(child)
		child.queue_free()


func edit_animation_root_node(blt_node:BLTAnimationNode):
	print("Setting root node")
	graph_node_stack = []
	active_graph_edit_index = -1
	truncate_graph_stack(0)
	
	blt_node.resource_name = "Root"
	
	if blt_node is BLTAnimationNodeBlendTree:
		animation_graph_root_node = blt_node
		push_graph_stack(blt_node)
		edit_graph(blt_node)
		return
	
	assert(is_instance_valid(animation_graph))
	
	push_warning("Cannot edit node %s. Graph type %s not yet supported." % [blt_node.resource_name, blt_node.get_class()])


func push_graph_stack(blt_node:BLTAnimationNode):
	graph_node_stack.append(blt_node)
	active_graph_edit_index = graph_node_stack.size() - 1
	
	var breadcrumb_button:Button = Button.new()
	breadcrumb_button_container.add_child(breadcrumb_button)
	breadcrumb_button.text = blt_node.resource_name
	breadcrumb_button.toggle_mode = true
	breadcrumb_button.set_meta("BLTAnimationNode", blt_node)
	breadcrumb_button.set_meta("graph_edit_index", active_graph_edit_index)
	breadcrumb_button.pressed.connect(on_breadcrumb_button_pressed.bind(active_graph_edit_index))


func truncate_graph_stack(level:int):
	graph_node_stack.resize(level)
	
	var is_above_stack_size = false
	for child in breadcrumb_button_container.get_children():
		if is_above_stack_size:
			breadcrumb_button_container.remove_child(child)
			child.queue_free()
			continue
		
		var button:Button = child as Button
		if not is_instance_valid(button):
			continue
		
		if button.get_meta("graph_edit_index") >= graph_node_stack.size():
			is_above_stack_size = true
			breadcrumb_button_container.remove_child(child)
			child.queue_free()


func on_breadcrumb_button_pressed(graph_edit_index:int):
	print("on_breadcrumb_button_pressed(%d)" % graph_edit_index)
	active_graph_edit_index = graph_edit_index
	update_breadcrumb_button_container()
	
	edit_graph(graph_node_stack[graph_edit_index])


func update_breadcrumb_button_container():
	for child in breadcrumb_button_container.get_children():
		var button:Button = child as Button
		if not is_instance_valid(button):
			continue
		
		if button.get_meta("graph_edit_index") == active_graph_edit_index:
			button.set_pressed_no_signal(true)
		else:
			button.set_pressed_no_signal(false)


func edit_graph(blt_node:BLTAnimationNode):
	if blt_node is BLTAnimationNodeBlendTree:
		reset_graph_control()
		var blend_tree_graph_edit:BltBlendTreeEditor = preload ("res://addons/blendalot/blend_tree_editor.tscn").instantiate()
		active_graph_control.add_child(blend_tree_graph_edit)
		blend_tree_graph_edit.edit_blend_tree(blt_node)
		blend_tree_graph_edit.edit_subgraph.connect(handle_subgraph_edit)
		active_graph_edit = blend_tree_graph_edit
	else:
		push_error("Cannot edit graph of node type %s" % blt_node.get_class())


func handle_subgraph_edit(blt_node:BLTAnimationNode):
	print("handling subgraph edit of node %s" % blt_node.resource_name)
	truncate_graph_stack(active_graph_edit_index + 1)
	push_graph_stack(blt_node)
	edit_graph(blt_node)
