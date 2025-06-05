#pragma once

#include <mpp/RenderSystem.h>
#include <mpp/ResourceManager.h>

#include <sdl/SDL.h>

#include "ImGuiBackendData.h"
#include "InputManager.h"


void imGuiSetup(mpp::RenderSystem* renderSystem, mpp::ResourceManager* resourceMgr, ImGuiBackendData* bd);

void imGuiShutdown(ImGuiBackendData* bd);

void imGuiNewFrame(SDL_Window* window, ImGuiBackendData* bd);

void imGuiHandleInput(InputManager* inputMgr, ImGuiBackendData* bd);
