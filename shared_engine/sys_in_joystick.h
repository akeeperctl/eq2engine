//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2015
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium joystick support brought by SDL
//////////////////////////////////////////////////////////////////////////////////

#ifndef SYS_IN_JOYSTICK_H
#define SYS_IN_JOYSTICK_H

#include "platform/Platform.h"

#define MAX_CONTROLLERS 4

class CEqGameControllerSDL
{
public:
	CEqGameControllerSDL() 
		: m_connected(false), m_gameCont(0), m_instanceId(-1), m_haptic(0)
	{
	}

	static void Init();
	static void Shutdown();

	static int ProcessEvent(SDL_Event* event);
	const char* GetName() const;

private:
	SDL_GameController*	m_gameCont;
	SDL_Haptic*			m_haptic;
	SDL_JoystickID		m_instanceId;
	bool				m_connected;

	static int GetControllerIndex(SDL_JoystickID instance);

	void Open(int device);
	void Close();
};

#endif // SYS_IN_JOYSTICK_H