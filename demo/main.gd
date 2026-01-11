extends Node3D

@onready var synced_animation_graph: SyncedAnimationGraph = %SyncedAnimationGraph
@onready var animation_tree: AnimationTree = %AnimationTree
@onready var blend_weight_slider: HSlider = %BlendWeightSlider
@onready var blend_weight_label: Label = %BlendWeightLabel

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	blend_weight_slider.value = 0.0

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass

func _on_blend_weight_slider_value_changed(value: float) -> void:
	animation_tree.set("parameters/Blend2/blend_amount", value)
	synced_animation_graph.set("parameters/AnimationBlend2Node/blend_amount", value)
	blend_weight_label.text = str(value)
