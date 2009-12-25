#ifndef OBJECT_EVENTS_HPP_INCLUDED
#define OBJECT_EVENTS_HPP_INCLUDED

enum OBJECT_EVENT_ID {
	OBJECT_EVENT_LOAD,
	OBJECT_EVENT_CREATE,
	OBJECT_EVENT_DONE_CREATE,
	OBJECT_EVENT_SURFACE_DAMAGE,
	OBJECT_EVENT_ENTER_ANIM,
	OBJECT_EVENT_END_ANIM,
	OBJECT_EVENT_COLLIDE_LEVEL,
	OBJECT_EVENT_COLLIDE_HEAD,
	OBJECT_EVENT_COLLIDE_FEET,
	OBJECT_EVENT_COLLIDE_DAMAGE,
	OBJECT_EVENT_COLLIDE,
	OBJECT_EVENT_JUMPED_ON,
	OBJECT_EVENT_GET_HIT,
	OBJECT_EVENT_PROCESS,
	OBJECT_EVENT_TIMER,
	OBJECT_EVENT_ENTER_WATER,
	OBJECT_EVENT_EXIT_WATER,
	OBJECT_EVENT_CHANGE_SOLID_DIMENSIONS_FAIL,
	OBJECT_EVENT_CHANGE_ANIMATION_FAILURE,
	OBJECT_EVENT_DIE,
	OBJECT_EVENT_HIT_PLAYER,
	OBJECT_EVENT_HIT_BY_PLAYER,
	OBJECT_EVENT_INTERACT,
	OBJECT_EVENT_CHILD_SPAWNED,
	OBJECT_EVENT_SPAWNED,
	NUM_OBJECT_BUILTIN_EVENT_IDS,
};

const std::string& get_object_event_str(int id);
int get_object_event_id(const std::string& str);

#endif
