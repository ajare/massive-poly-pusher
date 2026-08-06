#pragma once

#include <mpp/helper/FreeCamera.h>
#include <mpp/helper/FpsCamera.h>

#include "mpp/app/InputManager.h"

void updateFreeCamera(mpp::helper::FreeCamera& camera, InputManager* inputMgr, float frameTime);

void updateFpsCamera(mpp::helper::FpsCamera& camera, InputManager* inputMgr, float frameTime);
