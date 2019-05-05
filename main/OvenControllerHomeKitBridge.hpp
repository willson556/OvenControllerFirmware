#pragma once

#include "OvenController.hpp"

#define TAG "Oven"
#define ACCESSORY_NAME  TAG
#define MANUFACTURER_NAME   "TEW"
#define MODEL_NAME  "v1.1"
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

void hap_oven_initialize(OvenController &ovenController, uint8_t *mac);
void hap_oven_StateChangedHandler(OvenController::State newState);
