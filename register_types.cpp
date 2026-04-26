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

void initialize_blendalot_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(ExampleClass);

	//	ClassDB::register_class<BLTAnimationGraph>();
	//	ClassDB::register_class<BLTAnimationNode>();
	//	ClassDB::register_class<BLTAnimationNodeOutput>();
	//	ClassDB::register_class<BLTBlendTree>();
	//	ClassDB::register_class<BLTAnimationNodeSampler>();
	//	ClassDB::register_class<BLTAnimationNodeTimeScale>();
	//	ClassDB::register_class<BLTAnimationNodeBlend2>();
	//	ClassDB::register_class<BLTStateMachine>();
	//	ClassDB::register_class<BLTStateMachineTransition>();
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
