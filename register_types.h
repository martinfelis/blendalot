#ifdef BLENDALOT_MODULE
#include "modules/register_module_types.h"
#endif

#ifdef BLENDALOT_GDEXTENSION
#include <godot_cpp/core/class_db.hpp>
using namespace godot;
#endif

void initialize_blendalot_module(ModuleInitializationLevel p_level);
void uninitialize_blendalot_module(ModuleInitializationLevel p_level);
