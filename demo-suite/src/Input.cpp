#include "InputManager.h"

void InputManager::addEvent(const InputEvent& evt)
{
	mEvents.push_back(evt);
}
