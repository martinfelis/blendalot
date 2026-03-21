@tool
extends EditorPlugin
class_name BlendalotPlugin

var editor_dock:EditorDock = null
var animation_graph_editor:AnimationGraphEditor = null

func _enable_plugin() -> void:
	# Add autoloads here.
	pass


func _disable_plugin() -> void:
	# Remove autoloads here.
	pass


func _enter_tree() -> void:
	editor_dock = EditorDock.new()
	editor_dock.title = "Animation Graph"
	editor_dock.default_slot = EditorDock.DOCK_SLOT_BOTTOM
	animation_graph_editor = preload ("res://addons/blendalot/animation_graph_editor.tscn").instantiate()
	editor_dock.add_child(animation_graph_editor)
	add_dock(editor_dock)


func _exit_tree() -> void:
	remove_dock(editor_dock)
	editor_dock.queue_free()
	editor_dock = null
	
	animation_graph_editor.queue_free()
	animation_graph_editor = null


func _has_main_screen() -> bool:
	return false


func _make_visible(visible):
	pass


func _get_plugin_name():
	return "Blendalot"


func _get_plugin_icon():
	return EditorInterface.get_editor_theme().get_icon("Node", "EditorIcons")


func _handles(obj: Object) -> bool:
	return obj is BLTBlendTree


func _edit(object: Object):
	if not is_instance_valid(animation_graph_editor):
		push_error("Cannot edit object as AnimationGraphEditor is not initialized")
		return 
	
	if object is BLTBlendTree:
		animation_graph_editor.edit_animation_root_node(object)
		return
	
	if object is BLTStateMachine:
		animation_graph_editor.edit_animation_root_node(object)
		return
	
	print("Cannot (yet) edit object " + str(object))
