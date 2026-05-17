extends Node3D

@onready var blt_animation_graph: BLTAnimationGraph = %BLTAnimationGraph
@onready var animation_tree: AnimationTree = %AnimationTree
@onready var animation_player_synced: AnimationPlayer = %AnimationPlayerSynced

enum AnimationSource {	AnimPlayer, AnimTree, AnimGraph }

enum AnimationState { Idle, Walk, Run, Limp, TurnLeft90, TurnRight90}

var active_animation_source:AnimationSource = AnimationSource.AnimTree
var active_animation:AnimationState = AnimationState.Idle

var root_motion_translation:Vector3 = Vector3.ZERO
var root_motion_rotation:Quaternion = Quaternion.IDENTITY

var animation_state_to_name:Dictionary[AnimationState, String] = {
	AnimationState.Idle: "animation_library/Idle",
	AnimationState.Walk: "animation_library/Walk",
	AnimationState.Run: "animation_library/Run",
	AnimationState.Limp: "animation_library/Limp",
	AnimationState.TurnLeft90: "animation_library/TurnLeft90",
	AnimationState.TurnRight90: "animation_library/TurnRight90",
}

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	activate_animation_source(active_animation_source)

func activate_animation_source(animation_source:AnimationSource):
	if animation_source == AnimationSource.AnimTree:
		animation_player_synced.active = false
		blt_animation_graph.active = false
		animation_tree.active = true
	elif animation_source == AnimationSource.AnimGraph:
		animation_player_synced.active = true
		blt_animation_graph.active = false
		animation_tree.active = false		
	else:
		animation_player_synced.active = true
		blt_animation_graph.active = false
		animation_tree.active = false
	
	active_animation_source = animation_source

func update_animation_state_machine():
	if active_animation_source == AnimationSource.AnimTree:
		animation_tree["parameters/StateMachine/conditions/to_idle"] = false
		animation_tree["parameters/StateMachine/conditions/to_walk"] = false
		animation_tree["parameters/StateMachine/conditions/to_turnright"] = false
		animation_tree["parameters/StateMachine/conditions/to_turnleft"] = false
		
		if active_animation == AnimationState.Idle:
			animation_tree["parameters/StateMachine/conditions/to_idle"] = true
		elif active_animation == AnimationState.Walk:
			animation_tree["parameters/StateMachine/conditions/to_walk"] = true
		elif active_animation == AnimationState.TurnLeft90:
			animation_tree["parameters/StateMachine/conditions/to_turnleft"] = true
		elif active_animation == AnimationState.TurnRight90:
			animation_tree["parameters/StateMachine/conditions/to_turnright"] = true
	elif active_animation_source == AnimationSource.AnimGraph:
		blt_animation_graph["parameters/StateMachine/conditions/to_idle"] = false
		blt_animation_graph["parameters/StateMachine/conditions/to_walk"] = false
		blt_animation_graph["parameters/StateMachine/conditions/to_turnright"] = false
		blt_animation_graph["parameters/StateMachine/conditions/to_turnleft"] = false
		
		if active_animation == AnimationState.Idle:
			blt_animation_graph["parameters/StateMachine/conditions/to_idle"] = true
		elif active_animation == AnimationState.Walk:
			blt_animation_graph["parameters/StateMachine/conditions/to_walk"] = true
		elif active_animation == AnimationState.TurnLeft90:
			blt_animation_graph["parameters/StateMachine/conditions/to_turnright"] = true
		elif active_animation == AnimationState.TurnRight90:
			blt_animation_graph["parameters/StateMachine/conditions/to_turnleft"] = true
	else:
		animation_player_synced.play(animation_state_to_name[active_animation])

func query_root_motion_values():
	if active_animation_source == AnimationSource.AnimPlayer:
		root_motion_translation = animation_player_synced.get_root_motion_position()
		root_motion_rotation = animation_player_synced.get_root_motion_rotation()
	elif active_animation_source == AnimationSource.AnimTree:
		root_motion_translation = animation_tree.get_root_motion_position()
		root_motion_rotation = animation_tree.get_root_motion_rotation()
	elif active_animation_source == AnimationSource.AnimGraph:
		root_motion_translation = blt_animation_graph.get_root_motion_position()
		root_motion_rotation = blt_animation_graph.get_root_motion_rotation()
	else:
		root_motion_translation = Vector3.ZERO
		root_motion_rotation = Quaternion.IDENTITY

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	if Input.is_action_pressed("ui_up"):
		active_animation = AnimationState.Walk
	elif Input.is_action_pressed("ui_left"):
		active_animation = AnimationState.TurnLeft90
	elif Input.is_action_pressed("ui_right"):
		active_animation = AnimationState.TurnRight90
	else:
		active_animation = AnimationState.Idle

	update_animation_state_machine()
	
	query_root_motion_values()
	
	transform = transform.translated(get_quaternion() * root_motion_translation.rotated(Vector3.RIGHT, 3.141592 * 0.5) * 0.01)
	set_quaternion(get_quaternion() * root_motion_rotation)
