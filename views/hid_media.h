#pragma once

#include <gui/view.h>

typedef struct Hid Hid;
typedef struct HidMedia HidMedia;

HidMedia* hid_media_alloc(Hid* hid);

void hid_media_free(HidMedia* hid_media);

View* hid_media_get_view(HidMedia* hid_media);

void hid_media_set_connected_status(HidMedia* hid_media, bool connected);

// Update the on-screen indicators: "improved" shows the enhanced title, and
// "mouse_switch" swaps the Back icon to a mouse glyph (tap Back opens the mouse view).
void hid_media_set_mode(HidMedia* hid_media, bool improved, bool mouse_switch);
