#pragma once

#ifdef BLENDALOT_MODULE
#include "core/object/class_db.h"
#include <core/object/ref_counted.h>
#include <core/variant/variant.h>
#endif

#ifdef BLENDALOT_GDEXTENSION
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"
using namespace godot;
#endif

class ExampleClass : public RefCounted {
	GDCLASS(ExampleClass, RefCounted)

protected:
	static void _bind_methods();

public:
	ExampleClass() = default;
	~ExampleClass() override = default;

	void print_type(const Variant &p_variant) const;
};
