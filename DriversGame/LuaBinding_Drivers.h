//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2015
//////////////////////////////////////////////////////////////////////////////////
// Description: Driver Syndicate OOLua binding
//////////////////////////////////////////////////////////////////////////////////

#ifndef LUABINDING_DRIVERS
#define LUABINDING_DRIVERS

#include "luabinding/LuaBinding.h"
#include "luabinding/LuaBinding_Engine.h"

bool LuaBinding_InitDriverSyndicateBindings(lua_State* state);
bool LuaBinding_ConsoleHandler(const char* cmdText);

OOLUA::Script& GetLuaState();

#endif // LUABINDING_DRIVERS