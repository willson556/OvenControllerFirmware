#pragma once

#include "OvenController.hpp"

void hap_oven_initialize(OvenController &ovenController, uint8_t *mac);
void hap_oven_StateChangedHandler(OvenController::State newState);
