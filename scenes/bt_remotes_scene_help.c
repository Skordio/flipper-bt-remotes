#include "../bt_remotes.h"

// ---------------------------------------------------------------------------
// Help pages — one string per page, shown in a dialog_ex
// ---------------------------------------------------------------------------

static const char* const bt_remotes_help_pages[] = {
    // Page 1: profile select overview
    "Profile Select:\n"
    "Choose a profile to\n"
    "connect as that device.\n"
    "Hold OK to reorder.",

    // Page 2: remote type menu reordering
    "Remote Menu:\n"
    "Hold OK on any remote\n"
    "to drag it up/down.\n"
    "OK or Back to confirm.",

    // Page 3: hiding remote types via Settings
    "Hiding Remotes:\n"
    "Settings > Hide Remote\n"
    "Types to toggle items.\n"
    "[x]=shown  [ ]=hidden",
};

#define BT_REMOTES_HELP_PAGE_COUNT \
    (sizeof(bt_remotes_help_pages) / sizeof(bt_remotes_help_pages[0]))

// Scene state encodes the current page index
enum {
    BtRemotesHelpEventNext,
    BtRemotesHelpEventBack,
};

static void bt_remotes_scene_help_dialog_cb(DialogExResult result, void* context) {
    Hid* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesHelpEventNext);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesHelpEventBack);
    }
}

static void bt_remotes_scene_help_show_page(Hid* app, uint8_t page) {
    bool last = (page + 1 >= (uint8_t)BT_REMOTES_HELP_PAGE_COUNT);

    dialog_ex_reset(app->dialog);
    dialog_ex_set_text(app->dialog, bt_remotes_help_pages[page], 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog, "Back");
    dialog_ex_set_right_button_text(app->dialog, last ? "Done" : "Next");
    dialog_ex_set_result_callback(app->dialog, bt_remotes_scene_help_dialog_cb);
    dialog_ex_set_context(app->dialog, app);
}

void bt_remotes_scene_help_on_enter(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneHelp, 0);
    bt_remotes_scene_help_show_page(app, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewDialog);
}

bool bt_remotes_scene_help_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == BtRemotesHelpEventNext) {
            uint8_t page = (uint8_t)scene_manager_get_scene_state(
                app->scene_manager, BtRemotesSceneHelp);
            page++;
            if(page >= BT_REMOTES_HELP_PAGE_COUNT) {
                // Done — return to profile select
                scene_manager_previous_scene(app->scene_manager);
            } else {
                scene_manager_set_scene_state(
                    app->scene_manager, BtRemotesSceneHelp, page);
                bt_remotes_scene_help_show_page(app, page);
            }
        } else {
            // Back button
            scene_manager_previous_scene(app->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void bt_remotes_scene_help_on_exit(void* context) {
    Hid* app = context;
    dialog_ex_reset(app->dialog);
}
