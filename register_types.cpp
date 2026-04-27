#include "register_types.h"

#ifdef BLENDALOT_MODULE
#include "core/object/class_db.h"
#endif

#ifdef BLENDALOT_GDEXTENSION
#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
using namespace godot;
#endif

#include "src/example_class.h"

#include "blendalot_animation_graph.h"
#include "blendalot_animation_node.h"
#include "blendalot_blend_tree.h"
#include "blendalot_state_machine.h"

void initialize_blendalot_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(ExampleClass);
	GDREGISTER_CLASS(BLTAnimationNode);
	GDREGISTER_CLASS(BLTAnimationGraph);
	GDREGISTER_CLASS(BLTAnimationNodeOutput);
	GDREGISTER_CLASS(BLTBlendTree);
	GDREGISTER_CLASS(BLTAnimationNodeSampler);
	GDREGISTER_CLASS(BLTAnimationNodeTimeScale);
	GDREGISTER_CLASS(BLTAnimationNodeBlend2);
	GDREGISTER_CLASS(BLTStateMachine);
	GDREGISTER_CLASS(BLTStateMachineTransition);
}

void uninitialize_blendalot_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

#ifndef BLENDALOT_MODULE
extern "C" {
// Initialization
GDExtensionBool GDE_EXPORT blendalot_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_blendalot_module);
	init_obj.register_terminator(uninitialize_blendalot_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
#endif
