#include "../bt_remotes.h"

static void collection_pin_submenu_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Labels need to outlive the submenu (pointer, not copy). Use a static buffer.
static char pin_labels[BT_REMOTES_COLLECTION_MAX][BT_REMOTES_COLLECTION_NAME_LEN + 8];

static void build_pin_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Pin to Start");

    for(uint8_t i = 0; i < app->collection_count; i++) {
        bool pinned = false;
        for(uint8_t j = 0; j < app->pinned_count; j++) {
            if(strcmp(app->collection_names[i], app->pinned_collections[j]) == 0) {
                pinned = true;
                break;
            }
        }
        snprintf(
            pin_labels[i],
            sizeof(pin_labels[i]),
            "%s %s",
            pinned ? "[PIN]" : "[ ]  ",
            app->collection_names[i]);
        submenu_add_item(app->submenu, pin_labels[i], i, collection_pin_submenu_cb, app);
    }
}

void bt_remotes_scene_collection_pin_on_enter(void* context) {
    Hid* app = context;
    bt_remotes_collection_load_list(app);
    build_pin_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_collection_pin_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t idx = event.event;
        if(idx >= app->collection_count) return false;

        const char* name = app->collection_names[idx];

        // Find current pin position
        int8_t pin_pos = -1;
        for(uint8_t j = 0; j < app->pinned_count; j++) {
            if(strcmp(app->pinned_collections[j], name) == 0) {
                pin_pos = (int8_t)j;
                break;
            }
        }

        if(pin_pos >= 0) {
            // Unpin
            for(uint8_t j = (uint8_t)pin_pos + 1; j < app->pinned_count; j++) {
                strlcpy(
                    app->pinned_collections[j - 1],
                    app->pinned_collections[j],
                    BT_REMOTES_COLLECTION_NAME_LEN);
            }
            app->pinned_count--;
        } else if(app->pinned_count < BT_REMOTES_PINNED_MAX) {
            // Pin
            strlcpy(
                app->pinned_collections[app->pinned_count],
                name,
                BT_REMOTES_COLLECTION_NAME_LEN);
            app->pinned_count++;
        }

        bt_remotes_pinned_save(app);
        build_pin_submenu(app);
        submenu_set_selected_item(app->submenu, idx);
        return true;
    }

    return false;
}

void bt_remotes_scene_collection_pin_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
