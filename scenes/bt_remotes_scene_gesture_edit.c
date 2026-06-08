#include "../bt_remotes.h"
#include "../helpers/gesture_runner.h"

// Line-by-line gesture editor. Lines live in app->editing_gesture_lines and are
// persisted via bt_remotes_gesture_save on every change.
//
// scene state:
//   0          = submenu (lines + "+ Add line" + "Help")
//   1          = text input (add new or edit existing line; see s_edit_idx)
//   2          = per-line action menu (Edit / Delete) for s_selected
//   200        = help page
//   300+idx    = confirm delete of line idx

#define GE_STATE_SUBMENU    0
#define GE_STATE_INPUT      1
#define GE_STATE_LINEMENU   2
#define GE_STATE_HELP       200
#define GE_STATE_CONFIRM_BASE 300

#define GE_IDX_ADD  0xFE
#define GE_IDX_HELP 0xFD

// s_edit_idx: -1 = adding a new line, >=0 = editing that line index.
static int  s_edit_idx = -1;
static uint8_t s_selected = 0;
static char s_line_buf[GESTURE_LINE_LEN];

static const char* const gesture_help_text =
    "GESTURE COMMANDS\n"
    "One command per line:\n\n"
    "anchor tl|tr|bl|br\n"
    "  slam cursor to a corner\n"
    "move <dx> <dy>\n"
    "  relative mouse move (px)\n"
    "tap\n"
    "  left press + release\n"
    "click left|right\n"
    "drag <dx> <dy>\n"
    "  press, move, release\n"
    "press [left|right]\n"
    "  button down (hold)\n"
    "release [left|right]\n"
    "  button up\n"
    "scroll <n>\n"
    "wait <ms>\n"
    "key <combo>\n"
    "  e.g. key enter, key cmd space\n"
    "type <text>\n\n"
    "Tip: start with 'anchor' so moves\n"
    "are repeatable, then move/tap your\n"
    "way through the menus.";

static void gesture_edit_submenu_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void gesture_edit_text_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0xFF);
}

static bool gesture_edit_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    char err[32];
    if(!gesture_line_validate(text, err, sizeof(err))) {
        furi_string_set(error, err);
        return false;
    }
    return true;
}

static void gesture_edit_dialog_cb(DialogExResult result, void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result == DialogExResultRight ? 1 : 0);
}

// Submenu stores label pointers, not copies — needs a stable buffer per slot.
static char ge_labels[GESTURE_LINE_MAX][GESTURE_LINE_LEN + 8];

static void build_edit_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_gesture_name);
    for(uint8_t i = 0; i < app->editing_gesture_line_count; i++) {
        snprintf(
            ge_labels[i], sizeof(ge_labels[i]), "%u %s", (unsigned)(i + 1),
            app->editing_gesture_lines[i]);
        submenu_add_item(app->submenu, ge_labels[i], i, gesture_edit_submenu_cb, app);
    }
    submenu_add_item(app->submenu, "+ Add line", GE_IDX_ADD, gesture_edit_submenu_cb, app);
    submenu_add_item(app->submenu, "Help", GE_IDX_HELP, gesture_edit_submenu_cb, app);
}

static void build_line_menu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_gesture_lines[s_selected]);
    submenu_add_item(app->submenu, "Edit", 0, gesture_edit_submenu_cb, app);
    submenu_add_item(app->submenu, "Delete", 1, gesture_edit_submenu_cb, app);
}

static void show_text_input(Hid* app, const char* header) {
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input, gesture_edit_text_cb, app, s_line_buf, sizeof(s_line_buf), true);
    text_input_set_validator(app->text_input, gesture_edit_validator, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewTextInput);
}

void bt_remotes_scene_gesture_edit_on_enter(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_SUBMENU);
    build_edit_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_gesture_edit_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneGestureEdit);

    // ----- Text input (add or edit a line) -----
    if(state == GE_STATE_INPUT) {
        if(event.type == SceneManagerEventTypeCustom && event.event == 0xFF) {
            if(s_edit_idx < 0) {
                // Append a new line
                if(app->editing_gesture_line_count < GESTURE_LINE_MAX) {
                    strlcpy(
                        app->editing_gesture_lines[app->editing_gesture_line_count],
                        s_line_buf,
                        GESTURE_LINE_LEN);
                    app->editing_gesture_line_count++;
                }
            } else {
                strlcpy(
                    app->editing_gesture_lines[s_edit_idx], s_line_buf, GESTURE_LINE_LEN);
            }
            bt_remotes_gesture_save(app);
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_SUBMENU);
            build_edit_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        if(event.type == SceneManagerEventTypeBack) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_SUBMENU);
            build_edit_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        return false;
    }

    // ----- Help page -----
    if(state == GE_STATE_HELP) {
        if(event.type == SceneManagerEventTypeBack) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_SUBMENU);
            build_edit_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        return false;
    }

    // ----- Per-line action menu -----
    if(state == GE_STATE_LINEMENU) {
        if(event.type == SceneManagerEventTypeBack) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_SUBMENU);
            build_edit_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        if(event.type == SceneManagerEventTypeCustom) {
            if(event.event == 0) { // Edit
                s_edit_idx = s_selected;
                strlcpy(s_line_buf, app->editing_gesture_lines[s_selected], sizeof(s_line_buf));
                scene_manager_set_scene_state(
                    app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_INPUT);
                show_text_input(app, "Edit Command");
                return true;
            }
            if(event.event == 1) { // Delete -> confirm
                scene_manager_set_scene_state(
                    app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_CONFIRM_BASE + s_selected);
                dialog_ex_reset(app->dialog);
                dialog_ex_set_result_callback(app->dialog, gesture_edit_dialog_cb);
                dialog_ex_set_context(app->dialog, app);
                dialog_ex_set_header(app->dialog, "Remove line?", 64, 8, AlignCenter, AlignTop);
                dialog_ex_set_text(
                    app->dialog, app->editing_gesture_lines[s_selected], 64, 28, AlignCenter,
                    AlignTop);
                dialog_ex_set_left_button_text(app->dialog, "Cancel");
                dialog_ex_set_right_button_text(app->dialog, "Remove");
                view_dispatcher_switch_to_view(app->view_dispatcher, HidViewDialog);
                return true;
            }
        }
        return false;
    }

    // ----- Delete confirm -----
    if(state >= GE_STATE_CONFIRM_BASE) {
        uint8_t remove_idx = (uint8_t)(state - GE_STATE_CONFIRM_BASE);
        if(event.type == SceneManagerEventTypeCustom) {
            if(event.event == 1 && remove_idx < app->editing_gesture_line_count) {
                for(uint8_t j = remove_idx + 1; j < app->editing_gesture_line_count; j++) {
                    strlcpy(
                        app->editing_gesture_lines[j - 1],
                        app->editing_gesture_lines[j],
                        GESTURE_LINE_LEN);
                }
                app->editing_gesture_line_count--;
                bt_remotes_gesture_save(app);
            }
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_SUBMENU);
            dialog_ex_reset(app->dialog);
            build_edit_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        if(event.type == SceneManagerEventTypeBack) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_SUBMENU);
            dialog_ex_reset(app->dialog);
            build_edit_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        return false;
    }

    // ----- Submenu -----
    if(event.type == SceneManagerEventTypeBack) {
        // Return to the gesture library list
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, BtRemotesSceneGestureList);
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GE_IDX_ADD) {
            s_edit_idx     = -1;
            s_line_buf[0]  = '\0';
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_INPUT);
            show_text_input(app, "Add Command");
            return true;
        }
        if(event.event == GE_IDX_HELP) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_HELP);
            widget_reset(app->help_widget);
            widget_add_text_scroll_element(app->help_widget, 0, 0, 128, 64, gesture_help_text);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewHelp);
            return true;
        }
        if(event.event < app->editing_gesture_line_count) {
            s_selected = (uint8_t)event.event;
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_LINEMENU);
            build_line_menu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
    }
    return false;
}

void bt_remotes_scene_gesture_edit_on_exit(void* context) {
    Hid* app = context;
    text_input_reset(app->text_input);
    dialog_ex_reset(app->dialog);
    widget_reset(app->help_widget);
    submenu_reset(app->submenu);
    scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneGestureEdit, GE_STATE_SUBMENU);
}
