#include <string>
#include <exception>
#include "sdl/WindowSDL.h"

using namespace std;

WindowSDL::WindowSDL() :
	mWindow(nullptr)
{
}

SDL_Window* WindowSDL::getWindow()
{
	return mWindow;
}

void WindowSDL::create(int width, int height, bool fullScreen, bool vsync)
{
	mWidth = width;
	mHeight = height;
	mFullscreen = fullScreen;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	// Create window
	unsigned int windowFlags = SDL_WINDOW_OPENGL;

	if (fullScreen)
	{
		mWindow = SDL_CreateWindow("Demo Suite", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
	}
	else
	{
		mWindow = SDL_CreateWindow("Demo Suite", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL);
	}

	if (!mWindow)
	{
		string err = SDL_GetError();
		throw exception(("Could not create SDL window: " + err).c_str());
	}

#ifdef _DEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

	mContextGL = SDL_GL_CreateContext(mWindow);
	if (SDL_GL_MakeCurrent(mWindow, mContextGL) != 0)
	{
		throw exception("Could not set the OpenGL context.");
	}

	// Create renderer
	unsigned int rendererFlags = SDL_RENDERER_ACCELERATED;

	if (vsync)
	{
		rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
	}

//	SDL_ShowCursor(SDL_DISABLE);
}

void WindowSDL::destroy()
{
	SDL_GL_DeleteContext(mContextGL);
	SDL_DestroyWindow(mWindow);
}
	
void WindowSDL::setFullscreen(bool fullscreen)
{
	if (fullscreen)
	{
		SDL_SetWindowFullscreen(mWindow, SDL_WINDOW_FULLSCREEN);
	}
	else
	{
		SDL_SetWindowFullscreen(mWindow, 0);
	}

	mFullscreen = fullscreen;
}

void WindowSDL::setSize(int width, int height)
{
	// if we're fullscreen, then go to windowed first, to stop other windows in background being resized.
	if (mFullscreen)
	{
		setFullscreen(false);
		SDL_SetWindowSize(mWindow, width, height);
		setFullscreen(true);
	}
	else
	{
		SDL_SetWindowSize(mWindow, width, height);
	}
}

void WindowSDL::show()
{
	SDL_GL_SwapWindow(mWindow);
}

void WindowSDL::processEvents(InputManager* inputMgr)
{
	inputMgr->clearEvents();

	SDL_Event evt;
	while (SDL_PollEvent(&evt))
	{
		InputEvent ie;

		switch (evt.type)
		{
		case SDL_KEYDOWN:
			ie.type = IET_KeyPressed;
			ie.code = evt.key.keysym.sym;
			ie.key = evt.key.keysym.sym;
			ie.mod = evt.key.keysym.mod;
			inputMgr->addEvent(ie);
			break;

		case SDL_KEYUP:
			ie.type = IET_KeyReleased;
			ie.code = evt.key.keysym.scancode;
			ie.key = evt.key.keysym.sym;
			ie.mod = evt.key.keysym.mod;
			inputMgr->addEvent(ie);
			break;

		case SDL_MOUSEBUTTONDOWN:
			ie.type = IET_ButtonPressed;
			ie.b = evt.button.button;
			ie.code = evt.button.which;
			inputMgr->addEvent(ie);
			break;

		case SDL_MOUSEBUTTONUP:
			ie.type = IET_ButtonReleased;
			ie.b = evt.button.button;
			ie.code = evt.button.which;
			inputMgr->addEvent(ie);
			break;

		case SDL_MOUSEWHEEL:
			ie.type = IET_MouseWheel;
			ie.x = (float)-evt.wheel.x;
			ie.y = (float)evt.wheel.y;
			ie.code = evt.motion.which;
			inputMgr->addEvent(ie);
			break;

		case SDL_MOUSEMOTION:
			ie.type = IET_MouseMotion;
			ie.x = (float)evt.motion.x;
			ie.y = (float)evt.motion.y;
			ie.code = evt.motion.which;
			inputMgr->addEvent(ie);
			break;

		case SDL_TEXTINPUT:
			ie.type = IET_TextInput;
			strcpy(ie.s, evt.text.text);
			inputMgr->addEvent(ie);
			break;

		case SDL_WINDOWEVENT_ENTER:
			ie.type = IET_WindowEnter;
			inputMgr->addEvent(ie);
			break;

		case SDL_WINDOWEVENT_LEAVE:
			ie.type = IET_WindowExit;
			inputMgr->addEvent(ie);
			break;

		case SDL_WINDOWEVENT_FOCUS_GAINED:
			ie.type = IET_FocusGained;
			inputMgr->addEvent(ie);
			break;

		case SDL_WINDOWEVENT_FOCUS_LOST:
			ie.type = IET_FocusLost;
			inputMgr->addEvent(ie);
			break;

		case SDL_QUIT:
			exit(0);
			break;
		}
	}
}
