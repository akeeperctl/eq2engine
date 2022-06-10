//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Configuration file loader
//////////////////////////////////////////////////////////////////////////////////

#include "core/DebugInterface.h"
#include "core/ConCommandBase.h"
#include "core/IConsoleCommands.h"
#include "core/IFileSystem.h"
#include "core/ConVar.h"
#include "utils/strtools.h"

#include "input/InputCommandBinder.h"

void WriteCfgFile(const char *pszFilename, bool bWriteKeyConfiguration /*= true*/)
{
	IFile *cfgfile = g_fileSystem->Open(pszFilename,"w");
	if(!cfgfile)
	{
		MsgError("Failed to write configuraton file '%s'\n", pszFilename);
		return;
	}

	MsgInfo("Writing configuraton file '%s'\n",pszFilename);

	cfgfile->Print("// Auto-generated by WriteCfgFile()\n");

	if(bWriteKeyConfiguration)
		g_inputCommandBinder->WriteBindings(cfgfile);

	const Array<ConCommandBase*> *base = g_consoleCommands->GetAllCommands();

	for(int i = 0; i < base->numElem();i++)
	{
		if(base->ptr()[i]->IsConVar())
		{
			ConVar *cv = (ConVar*)base->ptr()[i];
			if(cv->GetFlags() & CV_ARCHIVE)
				cfgfile->Print("seti %s %s\n",cv->GetName(),cv->GetString());
		}
	}

	g_fileSystem->Close(cfgfile);
}

DECLARE_CMD(writecfg,"Saves the confirugation file", 0)
{
	if(CMD_ARGC > 0)
	{
		WriteCfgFile(CMD_ARGV(0).ToCString(),true);
	}
	else
		WriteCfgFile("user.cfg",true);
}
