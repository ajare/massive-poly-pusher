#pragma once

#include <sdl/SDL.h>
#include "Window.h"

class WindowSDL : public Window
{
	SDL_Window* mWindow;

	SDL_GLContext mContextGL;

	bool mFullscreen;

public:

	WindowSDL();

	SDL_Window* getWindow();

	void create(int width, int height, bool fullScreen, bool vsync);

	void destroy();

	void setFullscreen(bool fullscreen);

	void setSize(int width, int height);
	
	void show();

	void processEvents(InputManager* inputMgr);
};