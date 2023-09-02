#include <lua.hpp>
#include "core/core_common.h"

#include "esl.h"
#include "esl_luaref.h"


namespace esl
{
LuaRawRef::~LuaRawRef()
{
	Unref();
}

LuaRawRef::LuaRawRef(lua_State* L, int idx, int type)
	: m_state(L)
{
	if (lua_gettop(L) == 0)
		return;
	const int itmType = lua_type(L, idx);
	if(type == itmType)
	{
		lua_pushvalue(L, idx);
		m_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	}
}

LuaRawRef::LuaRawRef(const LuaRawRef& other)
{
	if (!other.IsValid())
		return;
	other.Push();
	m_state = other.m_state;
	m_ref = luaL_ref(m_state, LUA_REGISTRYINDEX);
}

LuaRawRef::LuaRawRef(LuaRawRef&& other) noexcept
	: m_state(other.m_state), m_ref(other.m_ref)
{
	other.m_state = nullptr;
	other.m_ref = LUA_NOREF;
}

LuaRawRef& LuaRawRef::operator=(const LuaRawRef& other)
{
	if (&other == this)
		return *this;

	Unref();
	m_state = other.m_state;
	if (!other.IsValid())
		return *this;

	other.Push();
	m_ref = luaL_ref(m_state, LUA_REGISTRYINDEX);
	return *this;
}

LuaRawRef& LuaRawRef::operator=(LuaRawRef&& other) noexcept
{
	if (&other == this)
		return *this;

	m_state = other.m_state;
	m_ref = other.m_ref;

	other.m_state = nullptr;
	other.m_ref = LUA_NOREF;

	return *this;
}

LuaRawRef& LuaRawRef::operator=(std::nullptr_t)
{
	Unref();
	return *this;
}

bool LuaRawRef::operator==(LuaRawRef const& rhs) const
{
	if (!IsValid() || !rhs.IsValid())
		return IsValid() == rhs.IsValid();
	
	if (m_state == rhs.m_state)
	{
		lua_rawgeti(m_state, LUA_REGISTRYINDEX, m_ref);
		lua_rawgeti(m_state, LUA_REGISTRYINDEX, rhs.m_ref);
		const bool result = !!lua_rawequal(m_state, -1, -2);
		lua_pop(m_state, 2);

		return result;
	}

	return false;
}

void LuaRawRef::Release()
{
	Unref();
}

// Pushes the referenced Lua value onto the stack
void LuaRawRef::Push() const
{
	if (IsValid())
		lua_rawgeti(m_state, LUA_REGISTRYINDEX, m_ref);
	else
		lua_pushnil(m_state);	
}

bool LuaRawRef::IsValid() const
{
	return m_state && m_ref != LUA_NOREF;
}

void LuaRawRef::Unref()
{
	if (!IsValid())
		return;
	luaL_unref(m_state, LUA_REGISTRYINDEX, m_ref);
	m_state = nullptr;
	m_ref = LUA_NOREF;
}

}
