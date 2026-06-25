extends EditorProperty
class_name RootMotionTrackEditor

var assign_button:Button = null
var clear_button:Button = null
var base_hint:NodePath

var filter_dialog:ConfirmationDialog = null
var tree:Tree = null


func _confirmed():
	var tree_item:TreeItem = tree.get_selected()
	
	if not is_instance_valid(tree_item):
		return
	
	var node_path:NodePath = NodePath(tree_item.get_metadata(0))
	
	emit_changed(get_edited_property(), node_path)
	update_property()
	filter_dialog.hide()


func _node_assign():
	var blt_animation_graph:BLTAnimationGraph = get_edited_object() as BLTAnimationGraph
	
	var animation_player:AnimationPlayer = blt_animation_graph.get_node(blt_animation_graph.animation_player) as AnimationPlayer

	if not is_instance_valid(animation_player):
		return

	var base:Node = animation_player.get_node(animation_player.root_node)

	var track_path_dict = {}

	for animation_name in animation_player.get_animation_list():
		var animation:Animation = animation_player.get_animation(animation_name) as Animation
		for track_idx in range(animation.get_track_count()):
			var node_path:String = animation.track_get_path(track_idx).get_concatenated_names()
			if not track_path_dict.has(node_path):
				track_path_dict[node_path] = 1
	
	tree.clear()
	var root:TreeItem = tree.create_item()
	var parenthood = {}
	
	for path:String in track_path_dict.keys():
		var node_path = NodePath(path)
		var tree_item:TreeItem = null
		var accum:String = ""
		
		for i in range(node_path.get_name_count()):
			var name = node_path.get_name(i)
			if not accum.is_empty():
				accum += "/"
			
			accum += name
			
			if not parenthood.has(accum):
				if is_instance_valid(tree_item):
					tree_item = tree.create_item(tree_item)
				else:
					tree_item = tree.create_item()
				
				parenthood[accum] = tree_item
				
				tree_item.set_text(0, name)
				tree_item.set_selectable(0, false);
				tree_item.set_editable(0, false);
				
				if base.has_node(accum):
					var node = base.get_node(accum)
					
					# TODO: not using the right icons, probably due to wrong theme name?
					if Engine.is_editor_hint():
						tree_item.set_icon(0, EditorInterface.get_editor_theme().get_icon(type_string(typeof(node)), "EditorIcons"))
			else:
				tree_item = parenthood[accum]
			
		var skeleton:Skeleton3D = base.get_node(accum) as Skeleton3D
		if not is_instance_valid(skeleton):
			continue
		
		var items = {}
		items[-1] = tree_item
		
		var bone_texture:Texture = null
		if Engine.is_editor_hint():
			# TODO: not using the right icons, probably due to wrong theme name?
			bone_texture = EditorInterface.get_editor_theme().get_icon("Bone", "EditorIcons")

		var bones_to_process:Array = skeleton.get_parentless_bones()

		while not bones_to_process.is_empty():
			var bone_index = bones_to_process[0]
			bones_to_process.remove_at(0)
			
			bones_to_process.append_array(skeleton.get_bone_children(bone_index))
			
			var parent_index:int = skeleton.get_bone_parent(bone_index)
			var parent_item:TreeItem = items.get(parent_index)
			
			var joint_item:TreeItem = tree.create_item(parent_item)
			items[bone_index] = joint_item
			
			joint_item.set_text(0, skeleton.get_bone_name(bone_index))
			joint_item.set_selectable(0, true)
			joint_item.set_icon(0, bone_texture)
			joint_item.set_metadata(0, accum + ":" + skeleton.get_bone_name(bone_index))
			joint_item.set_collapsed_recursive(true)
	
	tree.ensure_cursor_is_visible()
	var editor_scale = 1
	if Engine.is_editor_hint():
		editor_scale = EditorInterface.get_editor_scale()
	filter_dialog.popup_centered(Vector2i(500, 500) * editor_scale)


func _node_clear():
	emit_changed(get_edited_property(), NodePath())
	update_property()


func _init():
	var hbox_container = HBoxContainer.new()
	add_child(hbox_container)
	
	assign_button = Button.new()
	assign_button.accessibility_name = StringName("Assign")
	assign_button.size_flags_horizontal =Control.SIZE_EXPAND_FILL
	assign_button.clip_text = true
	assign_button.pressed.connect(_node_assign)
	hbox_container.add_child(assign_button)
	
	clear_button = Button.new()
	clear_button.accessibility_name = StringName("Clear")
	clear_button.pressed.connect(_node_clear)
	var clear_icon_texture:Texture = null
	if Engine.is_editor_hint():
		# TODO: not using the right icons, probably due to wrong theme name?
		clear_icon_texture = EditorInterface.get_editor_theme().get_icon("Clear", "EditorIcons")
	clear_button.icon = clear_icon_texture
	hbox_container.add_child(clear_button)
	
	filter_dialog = ConfirmationDialog.new()
	add_child(filter_dialog)
	
	filter_dialog.title = StringName("Select Root Bone")
	filter_dialog.confirmed.connect(_confirmed)
	
	tree = Tree.new()
	filter_dialog.add_child(tree)
	tree.auto_translate_mode = Node.AUTO_TRANSLATE_MODE_DISABLED
	tree.size_flags_vertical = Control.SIZE_EXPAND_FILL
	tree.hide_root = true
	tree.item_activated.connect(_confirmed)


func _update_property():
	var p = get_edited_property()
	var value = get_edited_object().get(p)

	assign_button.tooltip_text = value
	
	if value.is_empty():
		assign_button.text = "Assign..."
		assign_button.flat = false
		return
	
	assign_button.text = value
