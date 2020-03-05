//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Atlas packer - core and control
//////////////////////////////////////////////////////////////////////////////////

#include "core_base_header.h"
#include "DebugInterface.h"
#include "IVirtualStream.h"

#include "cmdlib.h"

void Usage()
{
	MsgWarning("USAGE:\n	texcooker -target <target name>\n");
}

extern void CookMaterialsToTarget(const char* pszTargetName);

int main(int argc, char* argv[])
{
	GetCore()->Init("texcooker", argc, argv);

	Install_SpewFunction();

	if(!g_fileSystem->Init(false))
		return -1;

	MsgInfo("TexCooker - Platform-Specific material/texture converter utility\n\n\n");

	if(g_cmdLine->GetArgumentCount() <= 1)
	{
		Usage();
	}

	for (int i = 0; i < g_cmdLine->GetArgumentCount(); i++)
	{
		const char* argStr = g_cmdLine->GetArgumentString(i);

		if (!stricmp(argStr, "-target"))
		{
			CookMaterialsToTarget(g_cmdLine->GetArgumentsOf(i));
		}
	}

	GetCore()->Shutdown();

	return 0;
}
