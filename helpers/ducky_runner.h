#pragma once

#include <furi.h>
#include <ble_profile/extra_profiles/hid_profile.h>

// Base directory on the SD card where BadKB/BadUSB scripts live
#define DUCKY_SCRIPT_DIR EXT_PATH("badusb")

typedef enum {
    DuckyRunnerStateIdle,
    DuckyRunnerStateRunning,
    DuckyRunnerStateDone,
    DuckyRunnerStateError,
} DuckyRunnerState;

typedef struct DuckyRunner DuckyRunner;

// Called from the runner worker thread when state changes (done / error).
// Use view_dispatcher_send_custom_event inside the callback to forward the
// notification back to the main thread.
typedef void (*DuckyRunnerCallback)(void* context);

DuckyRunner* ducky_runner_alloc(void);
void         ducky_runner_free(DuckyRunner* runner);

// Start executing the script at |path| through the supplied BLE HID profile.
// Returns false if a script is already running.
bool ducky_runner_start(DuckyRunner* runner, FuriHalBleProfileBase* profile, const char* path);

// Stop a running script (safe to call even if already stopped/done).
void ducky_runner_stop(DuckyRunner* runner);

DuckyRunnerState ducky_runner_get_state(const DuckyRunner* runner);

// Human-readable error string — valid only when state == DuckyRunnerStateError.
const char* ducky_runner_get_error(const DuckyRunner* runner);

void ducky_runner_set_callback(DuckyRunner* runner, DuckyRunnerCallback cb, void* context);
