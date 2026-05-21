#include "Camera.h"


void updateFpsCamera(mpp::helper::FpsCamera& camera, InputManager* inputMgr, float frameTime)
{
	if (inputMgr->keyDown(Key_W))
	{
		camera.forward(50.0f * frameTime);
	}
	if (inputMgr->keyDown(Key_S))
	{
		camera.backward(50.0f * frameTime);
	}
	if (inputMgr->keyDown(Key_A))
	{
		camera.left(50.0f * frameTime);
	}
	if (inputMgr->keyDown(Key_D))
	{
		camera.right(50.0f * frameTime);
	}
}


void updateFreeCamera(mpp::helper::FreeCamera& camera, InputManager* inputMgr, float frameTime)
{
	if (inputMgr->keyDown(Key_W))
	{
		camera.forward(50.0f * frameTime);
	}
	if (inputMgr->keyDown(Key_S))
	{
		camera.backward(50.0f * frameTime);
	}
	if (inputMgr->keyDown(Key_A))
	{
		camera.left(50.0f * frameTime);
	}
	if (inputMgr->keyDown(Key_D))
	{
		camera.right(50.0f * frameTime);
	}
	if (inputMgr->keyDown(Key_R))
	{
		camera.up(50.0f * frameTime);
	}
	if (inputMgr->keyDown(Key_V))
	{
		camera.down(50.0f * frameTime);
	}
	if (inputMgr->keyDown(Key_Q))
	{
		camera.roll(-60.0f * frameTime);
	}
	if (inputMgr->keyDown(Key_E))
	{
		camera.roll(60.0f * frameTime);
	}
}