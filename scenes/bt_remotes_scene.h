#pragma once

#include <gui/scene_manager.h>

// Generate scene id enum and total count
#define ADD_SCENE(prefix, name, id) BtRemotesScene##id,
typedef enum {
#include "bt_remotes_scenes.h"
    BtRemotesSceneNum,
} BtRemotesScene;
#undef ADD_SCENE

extern const SceneManagerHandlers bt_remotes_scene_handlers;

// Generate on_enter declarations
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "bt_remotes_scenes.h"
#undef ADD_SCENE

// Generate on_event declarations
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "bt_remotes_scenes.h"
#undef ADD_SCENE

// Generate on_exit declarations
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "bt_remotes_scenes.h"
#undef ADD_SCENE
