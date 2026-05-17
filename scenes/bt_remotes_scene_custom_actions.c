#include "../bt_remotes.h"
#include "../helpers/ducky_runner.h"
#include <storage/storage.h>

static void bt_remotes_scene_custom_actions_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Scan DUCKY_SCRIPT_DIR for .txt files and populate app->ducky_script_names.
static void bt_remotes_scene_custom_actions_scan(Hid* app) {
    app->ducky_script_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    dir     = storage_file_alloc(storage);

    if(!storage_dir_open(dir, DUCKY_SCRIPT_DIR)) {
        FURI_LOG_W("CustomActions", "Cannot open dir: %s", DUCKY_SCRIPT_DIR);
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FileInfo    fi;
    char        entry_name[DUCKY_SCRIPT_NAME_LEN];

    while(storage_dir_read(dir, &fi, entry_name, sizeof(entry_name))) {
        if(app->ducky_script_count >= DUCKY_MAX_SCRIPTS) break;
        if(file_info_is_dir(&fi)) continue;

        // Only list .txt files
        size_t len = strlen(entry_name);
        if(len < 5 || strcmp(entry_name + len - 4, ".txt") != 0) continue;

        // Store without the .txt extension so the menu label is clean
        strlcpy(
            app->ducky_script_names[app->ducky_script_count],
            entry_name,
            DUCKY_SCRIPT_NAME_LEN);
        app->ducky_script_names[app->ducky_script_count][len - 4] = '\0';
        app->ducky_script_count++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

void bt_remotes_scene_custom_actions_on_enter(void* context) {
    Hid* app = context;

    bt_remotes_scene_custom_actions_scan(app);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Custom Actions");

    if(app->ducky_script_count == 0) {
        // No scripts found — show a single disabled-looking placeholder
        submenu_add_item(app->submenu, "No scripts in /ext/badusb", 0xFF,
                         bt_remotes_scene_custom_actions_cb, app);
    } else {
        for(uint8_t i = 0; i < app->ducky_script_count; i++) {
            // ducky_script_names stores names without .txt — safe to use as menu label
            submenu_add_item(app->submenu, app->ducky_script_names[i], i,
                             bt_remotes_scene_custom_actions_cb, app);
        }
    }

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCustomActions));

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_custom_actions_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t idx = event.event;

        // 0xFF is the "no scripts" placeholder — ignore it
        if(idx == 0xFF || idx >= app->ducky_script_count) return true;

        scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneCustomActions, idx);

        // Build the full path for the selected script (re-append .txt stripped during scan)
        snprintf(
            app->pending_script_path,
            sizeof(app->pending_script_path),
            "%s/%s.txt",
            DUCKY_SCRIPT_DIR,
            app->ducky_script_names[idx]);

        scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomActionsRun);
        consumed = true;
    }

    return consumed;
}

void bt_remotes_scene_custom_actions_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
