#include "register_types.h"

#include "blendalot_animation_graph.h"
#include "core/object/class_db.h"

void initialize_blendalot_animgraph_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<BLTAnimationGraph>();
	ClassDB::register_class<BLTAnimationNode>();
	ClassDB::register_class<BLTAnimationNodeOutput>();
	ClassDB::register_class<BLTAnimationNodeBlendTree>();
	ClassDB::register_class<BLTAnimationNodeSampler>();
	ClassDB::register_class<BLTAnimationNodeTimeScale>();
	ClassDB::register_class<BLTAnimationNodeBlend2>();
}

void uninitialize_blendalot_animgraph_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// Nothing to do here in this example.
}