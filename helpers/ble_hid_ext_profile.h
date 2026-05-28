#pragma once
#include <ble_profile/extra_profiles/hid_profile.h>
#include <gap.h>

typedef struct {
    char name[FURI_HAL_BT_ADV_NAME_LENGTH];
    uint8_t mac[GAP_MAC_ADDR_SIZE]; /**< BLE public address; all zeros = hardware default */
    bool phone_kb_suppress; /**< true = advertise as mouse so iOS keeps its soft keyboard visible */
} BleProfileHidExtParams;

extern const FuriHalBleProfileTemplate* ble_profile_hid_ext;
