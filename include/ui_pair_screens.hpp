#pragma once
#include <lvgl.h>

// "How do you want to pair this display?"
//   1. From the app or website: shows a code + QR that the owner confirms
//   2. Pick a nearby gateway: choose it here, then type its 6-digit code
void build_pair_method_screen(bool fromSettings = false);
void build_claim_screen(bool fromSettings = false);
void cleanup_pair_screens();
