#include "bt_remotes_scene.h"

// Generate on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const bt_remotes_on_enter_handlers[])(void*) = {
#include "bt_remotes_scenes.h"
};
#undef ADD_SCENE

// Generate on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const bt_remotes_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "bt_remotes_scenes.h"
};
#undef ADD_SCENE

// Generate on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const bt_remotes_on_exit_handlers[])(void* context) = {
#include "bt_remotes_scenes.h"
};
#undef ADD_SCENE

const SceneManagerHandlers bt_remotes_scene_handlers = {
    .on_enter_handlers = bt_remotes_on_enter_handlers,
    .on_event_handlers = bt_remotes_on_event_handlers,
    .on_exit_handlers = bt_remotes_on_exit_handlers,
    .scene_num = BtRemotesSceneNum,
};
