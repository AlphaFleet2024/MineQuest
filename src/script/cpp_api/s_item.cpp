/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#include "cpp_api/s_item.h"
#include "cpp_api/s_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "lua_api/l_item.h"
#include "lua_api/l_inventory.h"
#include "server.h"
#include "log.h"
#include "util/pointedthing.h"
#include "inventory.h"
#include "inventorymanager.h"

bool ScriptApiItem::item_OnDrop(ItemStack &item,
		ServerActiveObject *dropper, v3f pos)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getItemCallback(item.name.c_str(), "on_drop"))
		return false;

	// Call function
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(L, dropper);
	pushFloatPos(L, pos);
	PCALL_RES(lua_pcall(L, 3, 1, error_handler));
	if (!lua_isnil(L, -1)) {
		try {
			item = read_item(L, -1, getServer()->idef());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 2);  // Pop item and error handler
	return true;
}

bool ScriptApiItem::item_OnPlace(ItemStack &item,
		ServerActiveObject *placer, const PointedThing &pointed)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getItemCallback(item.name.c_str(), "on_place"))
		return false;

	// Call function
	LuaItemStack::create(L, item);

	if (!placer)
		lua_pushnil(L);
	else
		objectrefGetOrCreate(L, placer);

	pushPointedThing(pointed);
	PCALL_RES(lua_pcall(L, 3, 1, error_handler));
	if (!lua_isnil(L, -1)) {
		try {
			item = read_item(L, -1, getServer()->idef());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 2);  // Pop item and error handler
	return true;
}

bool ScriptApiItem::item_OnWield(const ItemStack &item,
		ServerActiveObject *user)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getItemCallback(item.name.c_str(), "on_wield"))
		return false;

	// Call function
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(L, user);
	PCALL_RES(lua_pcall(L, 2, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
	return true;
}

bool ScriptApiItem::item_OnUnWield(const ItemStack &item,
		ServerActiveObject *user)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getItemCallback(item.name.c_str(), "on_unwield"))
		return false;

	// Call function
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(L, user);
	PCALL_RES(lua_pcall(L, 2, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
	return true;
}

bool ScriptApiItem::item_OnUse(ItemStack &item,
		ServerActiveObject *user, const PointedThing &pointed)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Push callback function on stack
	if (!getItemCallback(item.name.c_str(), "on_use"))
		return false;

	// Call function
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(L, user);
	pushPointedThing(pointed);
	PCALL_RES(lua_pcall(L, 3, 1, error_handler));
	if(!lua_isnil(L, -1)) {
		try {
			item = read_item(L, -1, getServer()->idef());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 2);  // Pop item and error handler
	return true;
}

bool ScriptApiItem::item_OnSecondaryUse(ItemStack &item,
		ServerActiveObject *user, const PointedThing &pointed)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	if (!getItemCallback(item.name.c_str(), "on_secondary_use"))
		return false;

	LuaItemStack::create(L, item);
	objectrefGetOrCreate(L, user);
	pushPointedThing(pointed);
	PCALL_RES(lua_pcall(L, 3, 1, error_handler));
	if (!lua_isnil(L, -1)) {
		try {
			item = read_item(L, -1, getServer()->idef());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 2);  // Pop item and error handler
	return true;
}

bool ScriptApiItem::item_OnCraft(ItemStack &item, ServerActiveObject *user,
		const InventoryList *old_craft_grid, const InventoryLocation &craft_inv)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "on_craft");
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(L, user);

	// Push inventory list
	std::vector<ItemStack> items;
	for (u32 i = 0; i < old_craft_grid->getSize(); i++) {
		items.push_back(old_craft_grid->getItem(i));
	}
	push_items(L, items);

	InvRef::create(L, craft_inv);
	PCALL_RES(lua_pcall(L, 4, 1, error_handler));
	if (!lua_isnil(L, -1)) {
		try {
			item = read_item(L, -1, getServer()->idef());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 2);  // Pop item and error handler
	return true;
}

bool ScriptApiItem::item_CraftPredict(ItemStack &item, ServerActiveObject *user,
		const InventoryList *old_craft_grid, const InventoryLocation &craft_inv)
{
	SCRIPTAPI_PRECHECKHEADER
	sanity_check(old_craft_grid);
	int error_handler = PUSH_ERROR_HANDLER(L);

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "craft_predict");
	LuaItemStack::create(L, item);
	objectrefGetOrCreate(L, user);

	//Push inventory list
	std::vector<ItemStack> items;
	for (u32 i = 0; i < old_craft_grid->getSize(); i++) {
		items.push_back(old_craft_grid->getItem(i));
	}
	push_items(L, items);

	InvRef::create(L, craft_inv);
	PCALL_RES(lua_pcall(L, 4, 1, error_handler));
	if (!lua_isnil(L, -1)) {
		try {
			item = read_item(L, -1, getServer()->idef());
		} catch (LuaError &e) {
			throw LuaError(std::string(e.what()) + ". item=" + item.name);
		}
	}
	lua_pop(L, 2);  // Pop item and error handler
	return true;
}

// Retrieves core.registered_items[name][callbackname]
// If that is nil or on error, return false and stack is unchanged
// If that is a function, returns true and pushes the
// function onto the stack
// If core.registered_items[name] doesn't exist, core.nodedef_default
// is tried instead so unknown items can still be manipulated to some degree
bool ScriptApiItem::getItemCallback(const char *name, const char *callbackname,
		const v3s16 *p)
{
	lua_State* L = getStack();

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_items");
	lua_remove(L, -2); // Remove core
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_getfield(L, -1, name);
	lua_remove(L, -2); // Remove registered_items
	// Should be a table
	if (lua_type(L, -1) != LUA_TTABLE) {
		// Report error and clean up
		errorstream << "Item \"" << name << "\" not defined";
		if (p)
			errorstream << " at position " << PP(*p);
		errorstream << std::endl;
		lua_pop(L, 1);

		// Try core.nodedef_default instead
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "nodedef_default");
		lua_remove(L, -2);
		luaL_checktype(L, -1, LUA_TTABLE);
	}

	setOriginFromTable(-1);

	lua_getfield(L, -1, callbackname);
	lua_remove(L, -2); // Remove item def
	// Should be a function or nil
	if (lua_type(L, -1) == LUA_TFUNCTION) {
		return true;
	}

	if (!lua_isnil(L, -1)) {
		errorstream << "Item \"" << name << "\" callback \""
			<< callbackname << "\" is not a function" << std::endl;
	}
	lua_pop(L, 1);
	return false;
}

void ScriptApiItem::pushPointedThing(const PointedThing &pointed, bool hitpoint)
{
	lua_State* L = getStack();

	push_pointed_thing(L, pointed, false, hitpoint);
}
