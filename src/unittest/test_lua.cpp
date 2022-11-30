/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2021 TurkeyMcMac, Jude Melton-Houghton <jwmhjwmh@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "test.h"
#include "config.h"
#include "script/lua_api/l_internal.h"

#include <stdexcept>

extern "C" {
#if USE_LUAJIT
	#include <luajit.h>
#else
	#include <lua.h>
#endif
#include <lauxlib.h>
#include <lualib.h>
}

/*
 * This class tests for two common issues that prevent correct error handling
 * between Lua and C++.
 * Further reading:
 * - https://luajit.org/extensions.html#exceptions
 * - http://lua-users.org/wiki/ErrorHandlingBetweenLuaAndCplusplus
 */

class TestLua : public TestBase
{
public:
	TestLua() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestLua"; }

	void runTests(IGameDef *gamedef);

	void testLuaDestructors();
	void testCxxExceptions();
#if USE_LUAJIT
	void testLuaJITFFILinking();
#endif
};

static TestLua g_test_instance;

void TestLua::runTests(IGameDef *gamedef)
{
	TEST(testLuaDestructors);
	TEST(testCxxExceptions);
#if USE_LUAJIT
	TEST(testLuaJITFFILinking);
#endif
}

////////////////////////////////////////////////////////////////////////////////

/*
	Check that Lua unwinds the stack correctly when it throws errors internally.
	(This is not the case with PUC Lua unless it was compiled as C++.)
*/

namespace
{

	class DestructorDetector {
		bool *did_destruct;
	public:
		DestructorDetector(bool *did_destruct) : did_destruct(did_destruct)
		{
			*did_destruct = false;
		}
		~DestructorDetector()
		{
			*did_destruct = true;
		}
	};

}

void TestLua::testLuaDestructors()
{
	bool did_destruct = false;

	lua_State *L = luaL_newstate();
	lua_cpcall(L, [](lua_State *L) -> int {
		DestructorDetector d(reinterpret_cast<bool*>(lua_touserdata(L, 1)));
		luaL_error(L, "error");
		return 0;
	}, &did_destruct);
	lua_close(L);

	UASSERT(did_destruct);
}

namespace {

	int wrapper(lua_State *L, lua_CFunction inner)
	{
		try {
			return inner(L);
		} catch (std::exception &e) {
			lua_pushstring(L, e.what());
			return lua_error(L);
		}
	}

}

/*
	Check that C++ exceptions are caught and re-thrown as Lua errors.
	This is handled by a wrapper we define ourselves.
	(PUC Lua does not support use of such a wrapper, we have a patched version)
*/

void TestLua::testCxxExceptions()
{
	lua_State *L = luaL_newstate();

#if USE_LUAJIT
	lua_pushlightuserdata(L, reinterpret_cast<void*>(wrapper));
	luaJIT_setmode(L, -1, LUAJIT_MODE_WRAPCFUNC | LUAJIT_MODE_ON);
	lua_pop(L, 1);
#else
	lua_atccall(L, wrapper);
#endif

	lua_pushcfunction(L, [](lua_State *L) -> int {
		throw std::runtime_error("example");
	});

	int caught = 0;
	std::string errmsg;
	try {
		if (lua_pcall(L, 0, 0, 0) != 0) {
			caught = 2;
			errmsg = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
		}
	} catch (std::exception &e) {
		caught = 1;
	}

	if (caught != 1)
		lua_close(L);

	UASSERTEQ(int, caught, 2);
	UASSERT(errmsg.find("example") != std::string::npos);
}

#if USE_LUAJIT
FFI_FCT(void, test_symbol) {}

void TestLua::testLuaJITFFILinking()
{
	lua_State *L = luaL_newstate();
	assert(L);
	try {
		luaL_openlibs(L);
		int result = luaL_dostring(L,
			"local has_ffi, ffi = pcall(require, 'ffi')\n"
			"if has_ffi then\n"
			"    ffi.cdef('void mtffi_test_symbol();')\n"
			"    assert(ffi.C.mtffi_test_symbol, 'Test symbol not found')\n"
			"end\n"
		);
		UTEST(result == 0, "Lua error: %s", luaL_checkstring(L, -1));
	} catch (...) {
		lua_close(L);
		throw;
	}
	lua_close(L);
}
#endif // USE_LUAJIT
