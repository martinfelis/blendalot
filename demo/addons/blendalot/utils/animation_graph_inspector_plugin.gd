extends EditorInspectorPlugin
class_name AnimationGraphInspectorPlugin

var RootMotionTrackEditor = preload("res://addons/blendalot/utils/root_motion_track_editor.gd")

func _can_handle(object):
	return object.get_class() == "BLTAnimationGraph"


func _parse_property(object, type, name, hint_type, hint_string, usage_flags, wide):
	# We handle properties of type integer.
	if type == TYPE_NODE_PATH and name == "root_motion_track":
		var editor:RootMotionTrackEditor = RootMotionTrackEditor.new()
		add_property_editor(name, editor)
		return true
	else:
		return false
