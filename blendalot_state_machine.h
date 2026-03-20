#pragma once

#include "blendalot_animation_node.h"

class BLTStateMachineTransition : public BLTAnimationNodeBlend2 {
	GDCLASS(BLTStateMachineTransition, BLTAnimationNodeBlend2);

	friend class BLTStateMachine;

	bool condition_override = false;
	float transition_time = 0.f;
	float transition_duration = 0.1;

public:
	bool evaluate_condition() {
		return condition_override;
	}

	void reset() {
		transition_time = 0.f;
		condition_override = false;
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
	const LocalVector<Ref<BLTAnimationNode>> empty_node_vector;

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

	void find_active_transition() {
		// Interruption is (not yet) allowed.
		if (active_transition.is_valid()) {
			return;
		}

		for (Ref<BLTStateMachineTransition> &transition : state_leaving_transitions[active_state_index]) {
			if (transition->evaluate_condition()) {
				activate_transition(transition);
				return;
			}
		}
	}

public:
	int64_t find_state_index(const Ref<BLTAnimationNode> &state) {
		return states.find(state);
	}

	int64_t find_transition_index(const Ref<BLTStateMachineTransition> &transition) {
		return transitions.find(transition);
	}

	void add_state(const Ref<BLTAnimationNode> &state) {
		states.push_back(state);
		state_leaving_transitions.push_back(LocalVector<Ref<BLTStateMachineTransition>>());

		if (entry_state.is_null()) {
			entry_state = state;
		}
	}

	bool add_transition(const Ref<BLTAnimationNode> &from_state, const Ref<BLTAnimationNode> &to_state) {
		int64_t from_state_index = find_state_index(from_state);
		if (from_state_index == -1) {
			print_error(vformat("Cannot add transition from %s to %s: from state not found in StateMachine.", from_state->get_name(), to_state->get_name()));
			return false;
		}

		if (find_state_index(to_state) == -1) {
			print_error(vformat("Cannot add transition from %s to %s: to state not found in StateMachine.", from_state->get_name(), to_state->get_name()));
			return false;
		}

		Ref<BLTStateMachineTransition> transition;
		transition.instantiate();

		transitions.push_back(transition);
		transition_states.push_back({ from_state, to_state });

		state_leaving_transitions[from_state_index].push_back(transition);

		return true;
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

		find_active_transition();

		if (active_transition.is_valid()) {
			active_transition->update_transition_time_and_weight(_graph_evaluation_context->graph_process_delta_time);
			active_transition->activate_inputs(transition_states[active_state_index]);

			if (transition_states[active_transition_index][0]->active) {
				transition_states[active_transition_index][0]->activate_inputs(empty_node_vector);
			}

			if (transition_states[active_transition_index][1]->active) {
				transition_states[active_transition_index][1]->activate_inputs(empty_node_vector);
			}
		} else {
			active_state->active = true;
		}
	}

	void calculate_sync_track(const LocalVector<Ref<BLTAnimationNode>> &input_nodes) override {
		GodotProfileZone("BLTStateMachine::calculate_sync_track");

		if (active_transition.is_valid()) {
			active_transition->calculate_sync_track(empty_node_vector);
		} else {
			active_state->calculate_sync_track(empty_node_vector);
		}
	}

	void update_time(double p_delta) override {
		GodotProfileZone("BLTStateMachine::update_time");

		if (active_transition.is_valid()) {
			active_transition->update_time(p_delta);
		} else {
			active_state->update_time(p_delta);
		}
	}

	void evaluate(GraphEvaluationContext &context, const LocalVector<AnimationData *> &input_datas, AnimationData &output_data) override {
		GodotProfileZone("BLTStateMachine::evaluate");

		if (active_transition.is_valid()) {
			active_transition->evaluate(context, input_datas, output_data);

			transition_states[active_transition_index][0]->active = false;
			transition_states[active_transition_index][1]->active = false;

			// Deactivate any finished transitions
			if (!Math::is_zero_approx(1. - active_transition->blend_weight)) {
				active_transition = nullptr;
				previous_state = nullptr;
			}
		} else {
			active_state->evaluate(context, input_datas, output_data);
			active_state->active = false;
		}
	}
};
