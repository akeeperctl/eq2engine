//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2014
//////////////////////////////////////////////////////////////////////////////////
// Description: Console variable factory/registrator
//////////////////////////////////////////////////////////////////////////////////

#ifndef _ICONCOMMANDFACTORY_H_
#define _ICONCOMMANDFACTORY_H_

#include "ds/Array.h"
#include "InterfaceManager.h"

class EqString;

class ConVar;
class ConCommand;
class ConCommandBase;

typedef bool (*cmdFilterFn_t)(ConCommandBase* pCmd, Array<EqString>& args);

#define CONSOLE_INTERFACE_VERSION		"CORE_ConsoleCommands_004"

class IConsoleCommands : public IEqCoreModule
{
public:
    friend class ConCommandBase;

    virtual void								RegisterCommand(ConCommandBase *pCmd) = 0;
    virtual void								UnregisterCommand(ConCommandBase *pCmd) = 0;

    virtual const	ConVar*						FindCvar(const char* name) = 0;
    virtual const	ConCommand*					FindCommand(const char* name) = 0;
    virtual const	ConCommandBase*				FindBase(const char* name) = 0;

    virtual	const	Array<ConCommandBase*>*	GetAllCommands() const = 0;

    // Executes file
    virtual void								ParseFileToCommandBuffer(const char* pszFilename) = 0;

    // Sets command buffer
    virtual void								SetCommandBuffer(const char* pszBuffer) = 0;

    // Appends to command buffer
    virtual void								AppendToCommandBuffer(const char* pszBuffer) = 0;

    // Clears to command buffer
    virtual void								ClearCommandBuffer() = 0;

    // Resets counter of same commands
    virtual void								ResetCounter() = 0;

    // Executes command buffer
    virtual bool								ExecuteCommandBuffer(cmdFilterFn_t filterFn = nullptr, bool quiet = false) = 0;

	// returns failed commands
	virtual Array<EqString>&					GetFailedCommands() = 0;	
};

INTERFACE_SINGLETON( IConsoleCommands, CConsoleCommands, CONSOLE_INTERFACE_VERSION, g_consoleCommands)

#endif //_ICONCOMMANDFACTORY_H_
