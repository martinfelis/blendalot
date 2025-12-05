#pragma once

#include "scene/animation/animation_player.h"

#include "core/io/resource.h"
#include "scene/3d/skeleton_3d.h"

#include <cassert>

struct GraphEvaluationContext {
	AnimationPlayer *animation_player = nullptr;
	Skeleton3D *skeleton_3d = nullptr;
};


struct AnimationData {
	enum TrackType : uint8_t {
		TYPE_VALUE, // Set a value in a property, can be interpolated.
		TYPE_POSITION_3D, // Position 3D track, can be compressed.
		TYPE_ROTATION_3D, // Rotation 3D track, can be compressed.
		TYPE_SCALE_3D, // Scale 3D track, can be compressed.
		TYPE_BLEND_SHAPE, // Blend Shape track, can be compressed.
		TYPE_METHOD, // Call any method on a specific node.
		TYPE_BEZIER, // Bezier curve.
		TYPE_AUDIO,
		TYPE_ANIMATION,
	};

	struct TrackValue {
		Animation::Track *track = nullptr;
		TrackType type = TYPE_ANIMATION;
	};

	struct PositionTrackValue : public TrackValue {
		int bone_idx = -1;
		Vector3 position = Vector3(0, 0, 0);
		PositionTrackValue() { type = TYPE_POSITION_3D; }
	};

	struct RotationTrackValue : public TrackValue {
		int bone_idx = -1;
		Quaternion rotation = Quaternion(0, 0, 0, 1);
		RotationTrackValue() { type = TYPE_ROTATION_3D; }
	};

	struct ScaleTrackValue : public TrackValue {
		int bone_idx = -1;
		Vector3 scale;
		ScaleTrackValue() { type = TYPE_SCALE_3D; }
	};

	AnimationData() = default;
	~AnimationData() {
		_clear_values();
	}

	void set_value(Animation::TypeHash thash, TrackValue *value) {
		if (!track_values.has(thash)) {
			track_values.insert(thash, value);
		} else {
			track_values[thash] = value;
		}
	}

	void clear() {
		_clear_values();
	}

	AHashMap<Animation::TypeHash, TrackValue *, HashHasher> track_values; // Animation::Track to TrackValue

protected:
	void _clear_values() {
		for (KeyValue<Animation::TypeHash, TrackValue *> &K : track_values) {
			memdelete(K.value);
		}
	}
};

struct SyncTrack {

};

class SyncedAnimationNode: public Resource {
	GDCLASS(SyncedAnimationNode, Resource);

	friend class SyncedAnimationGraph;

public:
	struct NodeTimeInfo {
		double length = 0.0;
		double position = 0.0;
		double sync_position = 0.0;
		double delta = 0.0;
		double sync_delta = 0.0;

		Animation::LoopMode loop_mode = Animation::LOOP_NONE;
		SyncTrack sync_track;
	};
	NodeTimeInfo node_time_info;

	struct InputSocket {
		StringName name;
		SyncedAnimationNode *node;
	};

	Vector<InputSocket> input_sockets;

	virtual ~SyncedAnimationNode() = default;
	virtual void initialize(GraphEvaluationContext &context) {}
	virtual void activate_inputs() {}
	virtual void calculate_sync_track() {}
	virtual void update_time(double p_delta) {
		node_time_info.delta = p_delta;
		node_time_info.position += p_delta;
		if (node_time_info.position > node_time_info.length) {
			switch (node_time_info.loop_mode) {
				case Animation::LOOP_NONE: {
					node_time_info.position = node_time_info.length;
					break;
				}
				case Animation::LOOP_LINEAR: {
					assert(node_time_info.length > 0.0);
					while (node_time_info.position > node_time_info.length) {
						node_time_info.position -= node_time_info.length;
					}
					break;
				}
				case Animation::LOOP_PINGPONG: {
					assert(false && !"Not yet implemented.");
					break;
				}
			}
		}
	}
	virtual void evaluate(GraphEvaluationContext &context, AnimationData &output) {}

	bool is_active() const { return active; }
	bool set_input_node(const StringName &socket_name, SyncedAnimationNode *node);
	void get_input_names(Vector<StringName> &inputs);

private:
	AnimationData *output = nullptr;
	bool active = false;
};

class AnimationSamplerNode : public SyncedAnimationNode {
	GDCLASS(AnimationSamplerNode, SyncedAnimationNode);

public:
	StringName animation_name;

private:
	Ref<Animation> animation;

	void initialize(GraphEvaluationContext &context) override;
	void evaluate(GraphEvaluationContext &context, AnimationData &output) override;
};

class BlendTree : public SyncedAnimationNode {
    struct Connection {
	    const SyncedAnimationNode* source_node = nullptr;
    	const SyncedAnimationNode* target_node = nullptr;
    	const StringName target_socket_name = "";
    };

	Vector<SyncedAnimationNode> nodes;
	Vector<int> node_parent;
	Vector<Connection> connections;

public:
	void connect_nodes(const SyncedAnimationNode* source_node, const SyncedAnimationNode* target_node, StringName target_socket_name) {
		// TODO
		// connections.append(Connection{source_node, target_node, target_socket_name});
		// sort_nodes_by_evaluation_order();
	}

	void sort_nodes_by_evaluation_order() {
		// TODO: sort nodes and node_parent s.t. for node i all children have index > i.
	}
};