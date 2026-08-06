#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>

#include <sdl/SDL.h>

#include "mpp/app/ImGuiBackendData.h"
#include "mpp/app/InputManager.h"


void imGuiSetup(mpp::RenderSystem* renderSystem, mpp::ResourceManager* resourceMgr, ImGuiBackendData* bd, bool enableDocking = false);

void imGuiShutdown(ImGuiBackendData* bd);

void imGuiNewFrame(SDL_Window* window, ImGuiBackendData* bd);

void imGuiHandleInput(InputManager* inputMgr, ImGuiBackendData* bd);
