#pragma once
#include <Arduino.h>
#include "button.h"

#define EP_BUTTON_1   1
#define EP_BUTTON_2   2

// genMultistateInput present_value:
//   0   — hold
//   1   — single
//   2   — double
//   3   — triple
//   255 — release (after hold)

void zigbeeInit();
void zigbeeLoop();
void zigbeeSendAction(uint8_t buttonIndex, ButtonEvent event);
bool zigbeeIsConnected();
int16_t zigbeeGetLinkQuality();  // 0-255, or -1 if no parent
