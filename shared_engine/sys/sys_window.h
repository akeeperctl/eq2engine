//////////////////////////////////////////////////////////////////////////////////
// Copyright © Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Drivers window handler
//////////////////////////////////////////////////////////////////////////////////

#ifndef SYS_WINDOW_H
#define SYS_WINDOW_H

void Sys_SetFullscreenMode(EQWNDHANDLE window);
void Sys_SetWindowedMode(EQWNDHANDLE window);

bool Host_Init();
void Host_GameLoop();
void Host_Terminate();

#endif // SYS_WINDOW_H
