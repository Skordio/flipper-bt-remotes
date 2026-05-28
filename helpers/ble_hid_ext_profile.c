#include "ble_hid_ext_profile.h"

#include <furi.h>
#include <ble/ble.h>

static FuriHalBleProfileBase* ble_profile_hid_ext_start(FuriHalBleProfileParams profile_params) {
    UNUSED(profile_params);
    return ble_profile_hid->start(NULL);
}

static void ble_profile_hid_ext_stop(FuriHalBleProfileBase* profile) {
    ble_profile_hid->stop(profile);
}

static void
    ble_profile_hid_ext_get_config(GapConfig* config, FuriHalBleProfileParams profile_params) {
    furi_check(config);
    furi_check(profile_params);
    BleProfileHidExtParams* hid_ext_profile_params = profile_params;

    ble_profile_hid->get_gap_config(config, NULL);

    if(hid_ext_profile_params->name[0] != '\0') {
        strlcpy(config->adv_name + 1, hid_ext_profile_params->name, sizeof(config->adv_name) - 1);
    }

    bool has_mac = false;
    for(size_t i = 0; i < GAP_MAC_ADDR_SIZE; i++) {
        if(hid_ext_profile_params->mac[i] != 0) {
            has_mac = true;
            break;
        }
    }
    if(has_mac) {
        memcpy(config->mac_address, hid_ext_profile_params->mac, GAP_MAC_ADDR_SIZE);
    }

    if(hid_ext_profile_params->phone_kb_suppress) {
        config->appearance_char = GAP_APPEARANCE_MOUSE;
    }
}

static const FuriHalBleProfileTemplate profile_callbacks = {
    .start = ble_profile_hid_ext_start,
    .stop = ble_profile_hid_ext_stop,
    .get_gap_config = ble_profile_hid_ext_get_config,
};

const FuriHalBleProfileTemplate* ble_profile_hid_ext = &profile_callbacks;
