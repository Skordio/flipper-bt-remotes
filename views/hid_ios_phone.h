#pragma once

#include <gui/view.h>

typedef struct Hid Hid;
typedef struct HidIosPhone HidIosPhone;

HidIosPhone* hid_ios_phone_alloc(Hid* bt_hid);
void hid_ios_phone_free(HidIosPhone* hid_ios_phone);
View* hid_ios_phone_get_view(HidIosPhone* hid_ios_phone);
void hid_ios_phone_set_connected_status(HidIosPhone* hid_ios_phone, bool connected);
