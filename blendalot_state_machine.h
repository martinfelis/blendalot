#pragma once

#include "blendalot_animation_node.h"

class BLTStateMachineTransition : public BLTAnimationNodeBlend2 {
	GDCLASS(BLTStateMachineTransition, BLTAnimationNodeBlend2);

	StringName transition_duration_name = PNAME("transition_duration");

	friend class BLTStateMachine;

	bool is_transition_forced = false;
	double transition_time = 0.f;
	double transition_duration = 0.1;

protected:
	static void _bind_methods();
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _get(const StringName &p_name, Variant &r_ret) const;
	bool _set(const StringName &p_name, const Variant &p_value);

public:
	void force_transition(bool value) {
		is_transition_forced = value;
	}

	void set_transition_duration(double value) {
		transition_duration = value;
	}

	double get_transition_duration() const {
		return transition_duration;
	}

	double get_transition_time() const {
		return transition_time;
	}

	bool evaluate_condition() {
		if (is_transition_forced) {
			is_transition_forced = false;
			return true;
		}

		return false;
	}

	void reset() {
		transition_time = 0.f;
		is_transition_forced = false;
	}

	void update_transition_time_and_weight(double p_delta) {
		transition_time = CLAMP(transition_time + p_delta, 0., transition_duration);
		blend_weight = transition_time / transition_duration;
	}

	bool initialize(GraphEvaluationContext &context) override {
		reset();

		if (!BLTAnimationNode::initialize(context)) {
			return false;
		}

		return true;
	}
};

class BLTStateMachine : public BLTAnimationNode {
	GDCLASS(BLTStateMachine, BLTAnimationNode);

	GraphEvaluationContext *_graph_evaluation_context = nullptr;

	LocalVector<Ref<BLTAnimationNode>> states;
	LocalVector<Ref<BLTAnimationNode>> transitions;

	// must be sorted by priority
	LocalVector<LocalVector<Ref<BLTStateMachineTransition>>> state_leaving_transitions;

	LocalVector<LocalVector<Ref<BLTAnimationNode>>> transition_states;

	Ref<BLTAnimationNode> entry_state;
	Ref<BLTAnimationNode> active_state;
	Ref<BLTAnimationNode> previous_state;
	int active_state_index = -1;

	Ref<BLTStateMachineTransition> active_transition;
	int active_transition_index = -1;

	void activate_state(const Ref<BLTAnimationNode> &state) {
		active_state_index = states.find(state);
		assert(active_state_index != -1);

		active_state = state;
	}

	void activate_transition(const Ref<BLTStateMachineTransition> &transition) {
		transition->reset();
		active_transition = transition;
		active_transition_index = find_transition_index(transition);

		previous_state = active_state;
		activate_state(transition_states[active_transition_index][1]);
	}

	void find_and_activate_transition() {
		for (Ref<BLTStateMachineTransition> &transition : state_leaving_transitions[active_state_index]) {
			if (transition->evaluate_condition()) {
				activate_transition(transition);
				return;
			}
		}
	}

protected:
	static void _bind_methods();
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;

public:
	Vector2 graph_offset;

	Vector2 get_graph_offset() const {
		return graph_offset;
	}

	void set_graph_offset(const Vector2 &p_graph_offset) {
		graph_offset = p_graph_offset;
	}

	int64_t find_state_index(const Ref<BLTAnimationNode> &state) const {
		return states.find(state);
	}

	int64_t find_state_index_by_name(const StringName &name) const {
		for (uint32_t i = 0; i < states.size(); i++) {
			if (states[i]->get_name() == name) {
				return i;
			}
		}
		return -1;
	}

	int64_t find_transition_index(const Ref<BLTStateMachineTransition> &transition) const {
		return transitions.find(transition);
	}

	Ref<BLTStateMachineTransition> get_transition_by_index(int64_t index) const {
		if (index < 0 || index >= transitions.size()) {
			return nullptr;
		}

		return transitions[index];
	}

	Ref<BLTStateMachineTransition> get_active_transition() const {
		return active_transition;
	}

	void add_state(const Ref<BLTAnimationNode> &state) {
		StringName node_base_name = state->get_name();
		if (node_base_name.is_empty()) {
			node_base_name = state->get_class_name();
		}
		state->set_name(node_base_name);

		int number_suffix = 1;
		while (find_state_index_by_name(state->get_name()) != -1) {
			state->set_name(vformat("%s %d", node_base_name, number_suffix));
			number_suffix++;
		}

		states.push_back(state);
		state_leaving_transitions.push_back(LocalVector<Ref<BLTStateMachineTransition>>());

		if (entry_state.is_null()) {
			entry_state = state;
		}
	}

	TypedArray<StringName> get_state_names_as_typed_array() const {
		Vector<StringName> vec;
		for (const Ref<BLTAnimationNode> &state : states) {
			vec.push_back(state->get_name());
		}

		TypedArray<StringName> typed_arr;
		typed_arr.resize(vec.size());
		for (uint32_t i = 0; i < vec.size(); i++) {
			typed_arr[i] = vec[i];
		}
		return typed_arr;
	}

	Ref<BLTAnimationNode> get_state(const StringName &state_name) const {
		int state_index = find_state_index_by_name(state_name);

		if (state_index >= 0) {
			return states[state_index];
		}

		return nullptr;
	}

	bool add_transition(const Ref<BLTAnimationNode> &from_state, const Ref<BLTAnimationNode> &to_state, const Ref<BLTStateMachineTransition> &transition) {
		int64_t from_state_index = find_state_index(from_state);
		if (from_state_index == -1) {
			print_error(vformat("Cannot add transition from %s to %s: from state not found in StateMachine.", from_state->get_name(), to_state->get_name()));
			return false;
		}

		if (find_state_index(to_state) == -1) {
			print_error(vformat("Cannot add transition from %s to %s: to state not found in StateMachine.", from_state->get_name(), to_state->get_name()));
			return false;
		}

		if (find_transition_index(transition) != -1) {
			print_error(vformat("Cannot add transition: transition already added."));
			return false;
		}

		transitions.push_back(transition);
		transition_states.push_back({ from_state, to_state });

		state_leaving_transitions[from_state_index].push_back(transition);

		return true;
	}

	Array get_transitions_as_array() const {
		Array result;

		for (uint32_t i = 0; i < transitions.size(); i++) {
			result.push_back(transition_states[i][0]);
			result.push_back(transition_states[i][1]);
			result.push_back(transitions[i]);
		}

		return result;
	}

	// overrides from BLTAnimationNode
	bool initialize(GraphEvaluationContext &context) override {
		GodotProfileZone("BLTStateMachine::initialize");

		_graph_evaluation_context = &context;

		bool has_failed_state = true;
		for (Ref<BLTAnimationNode> state : states) {
			if (!state->initialize(context)) {
				has_failed_state = true;
			}
		}

		active_transition = nullptr;
		active_state_index = -1;
		active_transition_index = -1;

		return !has_failed_state;
	}

	void
	activate_inputs(const LocalVector<Ref<BLTAnimationNode>> &input_nodes) override {
		GodotProfileZone("BLTStateMachine::activate_inputs");

		if (active_state_index == -1) {
			activate_state(entry_state);
		}

		// Interruption is (not yet) allowed.
		if (!active_transition.is_valid()) {
			find_and_activate_transition();
		}

		if (active_transition.is_valid()) {
			active_transition->update_transition_time_and_weight(_graph_evaluation_context->graph_process_delta_time);
			active_transition->activate_inputs(transition_states[active_transition_index]);

			if (previous_state->active) {
				transition_states[active_transition_index][1]->activate_inputs({});
			}
			if (active_state->active) {
				transition_states[active_transition_index][0]->activate_inputs({});
			}

		} else {
			active_state->active = true;
			active_state->activate_inputs({});
		}
	}

	void calculate_sync_track(const LocalVector<Ref<BLTAnimationNode>> &input_nodes) override {
		GodotProfileZone("BLTStateMachine::calculate_sync_track");

		if (active_transition.is_valid()) {
			if (previous_state->active) {
				previous_state->calculate_sync_track({});
			}

			if (active_state->active) {
				active_state->calculate_sync_track({});
			}

			active_transition->calculate_sync_track(transition_states[active_transition_index]);
		} else {
			active_state->calculate_sync_track({});
		}
	}

	void update_time(double p_delta) override {
		GodotProfileZone("BLTStateMachine::update_time");

		if (active_transition.is_valid()) {
			if (previous_state->active) {
				previous_state->update_time(p_delta);
			}

			if (active_state->active) {
				active_state->update_time(p_delta);
			}

			active_transition->update_time(p_delta);
		} else {
			active_state->update_time(p_delta);
		}
	}

	void evaluate(GraphEvaluationContext &context, const LocalVector<AnimationData *> &input_datas, AnimationData &output_data) override {
		GodotProfileZone("BLTStateMachine::evaluate");

		if (active_transition.is_valid()) {
			AnimationData *active_state_data = nullptr;
			AnimationData *previous_state_data = nullptr;

			if (previous_state->active) {
				previous_state_data = context.animation_data_allocator.allocate();
				previous_state->evaluate(context, LocalVector<AnimationData *>(), *previous_state_data);
			}

			if (active_state->active) {
				active_state_data = context.animation_data_allocator.allocate();
				active_state->evaluate(context, LocalVector<AnimationData *>(), *active_state_data);
			}

			active_transition->evaluate(context, { previous_state_data, active_state_data }, output_data);

			if (previous_state->active) {
				context.animation_data_allocator.free(previous_state_data);
				previous_state->active = false;
			}

			if (active_state->active) {
				context.animation_data_allocator.free(active_state_data);
				active_state->active = false;
			}

			// Deactivate any finished transitions
			if (Math::is_zero_approx(1. - active_transition->blend_weight)) {
				previous_state = nullptr;
				active_transition = nullptr;
			}
		} else {
			active_state->evaluate(context, input_datas, output_data);
			active_state->active = false;
		}
	}
};
