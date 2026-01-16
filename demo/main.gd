extends Node3D

@onready var mixamo_amy_walk_limp: Node3D = %MixamoAmyWalkLimp
@onready var mixamo_amy_walk_limp_synced: Node3D = %MixamoAmyWalkLimpSynced
@onready var mixamo_amy_walk_run: Node3D = %MixamoAmyWalkRun
@onready var mixamo_amy_walk_run_synced: Node3D = %MixamoAmyWalkRunSynced

@onready var blend_weight_slider: HSlider = %BlendWeightSlider
@onready var blend_weight_label: Label = %BlendWeightLabel

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	blend_weight_slider.value = 0.5

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass

func _on_blend_weight_slider_value_changed(value: float) -> void:
	mixamo_amy_walk_limp.get_node("AnimationTree").set("parameters/Blend2/blend_amount", value)
	mixamo_amy_walk_limp_synced.get_node("SyncedAnimationGraph").set("parameters/AnimationBlend2Node/blend_amount", value)

	mixamo_amy_walk_run.get_node("AnimationTree").set("parameters/Blend2/blend_amount", value)
	mixamo_amy_walk_run_synced.get_node("SyncedAnimationGraph").set("parameters/AnimationBlend2Node/blend_amount", value)

	blend_weight_label.text = str(value)
