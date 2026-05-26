#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Input slot enum — 11 physical inputs, in display order
// ---------------------------------------------------------------------------

typedef enum {
    CustomRemoteInputTapUp = 0,
    CustomRemoteInputTapDown,
    CustomRemoteInputTapLeft,
    CustomRemoteInputTapRight,
    CustomRemoteInputHoldUp,
    CustomRemoteInputHoldDown,
    CustomRemoteInputHoldLeft,
    CustomRemoteInputHoldRight,
    CustomRemoteInputTapOk,
    CustomRemoteInputHoldOk,
    CustomRemoteInputTapBack,
    CustomRemoteInputCount, // sentinel — always 11
} CustomRemoteInputSlot;

// Human-readable labels for the edit screen (indexed by CustomRemoteInputSlot).
// Defined once in hid_custom_remote.c; use this extern declaration everywhere else.
extern const char* const custom_remote_input_labels[CustomRemoteInputCount];

// ---------------------------------------------------------------------------
// Remote definition — stored in/loaded from .remote files
// ---------------------------------------------------------------------------

#define BT_REMOTES_CUSTOM_REMOTE_NAME_LEN   32
#define BT_REMOTES_CUSTOM_REMOTE_SCRIPT_LEN 256

typedef struct {
    char name[BT_REMOTES_CUSTOM_REMOTE_NAME_LEN];
    char scripts[CustomRemoteInputCount][BT_REMOTES_CUSTOM_REMOTE_SCRIPT_LEN];
} CustomRemoteDef;

// ---------------------------------------------------------------------------
// HidCustomRemote view — public API
// ---------------------------------------------------------------------------

typedef struct HidCustomRemote HidCustomRemote;

// Callback fired when the user presses a mapped input (short-press or long-press).
// Long Back is NOT fired — it exits the view instead.
// Returns true if the event was handled (script ran), false if the slot is unassigned.
// For Short Back specifically, returning false causes the view to propagate the event
// so the scene manager can pop the scene (i.e. unassigned TapBack exits the view).
typedef bool (*HidCustomRemoteCallback)(void* context, CustomRemoteInputSlot slot);

// Extract the filename stem from a full path into out (truncated to out_size-1).
// Empty path → "-".  Shared with edit scene to avoid duplication.
void hid_custom_remote_stem(const char* path, char* out, size_t out_size);

HidCustomRemote* hid_custom_remote_alloc(void);
void             hid_custom_remote_free(HidCustomRemote* view);
View*            hid_custom_remote_get_view(HidCustomRemote* view);

// Populate the view from a remote definition. Derives short labels from script paths
// (filename stem, truncated to fit). Call before switching to this view.
void hid_custom_remote_set_remote(HidCustomRemote* view, const CustomRemoteDef* def);

void hid_custom_remote_set_callback(
    HidCustomRemote*        view,
    HidCustomRemoteCallback cb,
    void*                   context);
