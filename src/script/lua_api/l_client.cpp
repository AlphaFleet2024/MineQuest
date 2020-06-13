/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

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

#include <set>
#include "l_client.h"
#include "chatmessage.h"
#include "client/client.h"
#include "client/clientevent.h"
#include "client/sound.h"
#include "client/clientenvironment.h"
#include "client/renderingengine.h"
#include "common/c_content.h"
#include "common/c_converter.h"
#include "cpp_api/s_base.h"
#include "gettext.h"
#include "l_internal.h"
#include "lua_api/l_nodemeta.h"
#include "gui/mainmenumanager.h"
#include "map.h"
#include "util/string.h"
#include "nodedef.h"
#include "filesys.h"
#include "util/string.h"
#include "content/mods.h"

#define checkCSMRestrictionFlag(flag) \
	( getClient(L)->checkCSMRestrictionFlag(CSMRestrictionFlags::flag) )

// Not the same as FlagDesc, which contains an `u32 flag`
struct CSMFlagDesc {
	const char *name;
	u64 flag;
};

/*
	FIXME: This should eventually be moved somewhere else
	It also needs to be kept in sync with the definition of CSMRestrictionFlags
	in network/networkprotocol.h
*/
const static CSMFlagDesc flagdesc_csm_restriction[] = {
	{"load_client_mods",  CSM_RF_LOAD_CLIENT_MODS},
	{"chat_messages",     CSM_RF_CHAT_MESSAGES},
	{"read_itemdefs",     CSM_RF_READ_ITEMDEFS},
	{"read_nodedefs",     CSM_RF_READ_NODEDEFS},
	{"lookup_nodes",      CSM_RF_LOOKUP_NODES},
	{"read_playerinfo",   CSM_RF_READ_PLAYERINFO},
	{NULL,      0}
};

// get_current_modname()
int ModApiClient::l_get_current_modname(lua_State *L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_CURRENT_MOD_NAME);
	return 1;
}

// get_modpath(modname)
int ModApiClient::l_get_modpath(lua_State *L)
{
	std::string modname = readParam<std::string>(L, 1);
	// Client mods use a virtual filesystem, see Client::scanModSubfolder()
	std::string path = modname + ":";
	lua_pushstring(L, path.c_str());
	return 1;
}

// get_last_run_mod()
int ModApiClient::l_get_last_run_mod(lua_State *L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_CURRENT_MOD_NAME);
	std::string current_mod = readParam<std::string>(L, -1, "");
	if (current_mod.empty()) {
		lua_pop(L, 1);
		lua_pushstring(L, getScriptApiBase(L)->getOrigin().c_str());
	}
	return 1;
}

// set_last_run_mod(modname)
int ModApiClient::l_set_last_run_mod(lua_State *L)
{
	if (!lua_isstring(L, 1))
		return 0;

	const char *mod = lua_tostring(L, 1);
	getScriptApiBase(L)->setOriginDirect(mod);
	lua_pushboolean(L, true);
	return 1;
}

// print(text)
int ModApiClient::l_print(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	std::string text = luaL_checkstring(L, 1);
	rawstream << text << std::endl;
	return 0;
}

// display_chat_message(message)
int ModApiClient::l_display_chat_message(lua_State *L)
{
	if (!lua_isstring(L, 1))
		return 0;

	std::string message = luaL_checkstring(L, 1);
	getClient(L)->pushToChatQueue(new ChatMessage(utf8_to_wide(message)));
	lua_pushboolean(L, true);
	return 1;
}

// send_chat_message(message)
int ModApiClient::l_send_chat_message(lua_State *L)
{
	if (!lua_isstring(L, 1))
		return 0;

	// If server disabled this API, discard

	if (checkCSMRestrictionFlag(CSM_RF_CHAT_MESSAGES))
		return 0;

	std::string message = luaL_checkstring(L, 1);
	getClient(L)->sendChatMessage(utf8_to_wide(message));
	return 0;
}

// clear_out_chat_queue()
int ModApiClient::l_clear_out_chat_queue(lua_State *L)
{
	getClient(L)->clearOutChatQueue();
	return 0;
}

// get_player_names()
int ModApiClient::l_get_player_names(lua_State *L)
{
	if (checkCSMRestrictionFlag(CSM_RF_READ_PLAYERINFO))
		return 0;

	const std::list<std::string> &plist = getClient(L)->getConnectedPlayerNames();
	lua_createtable(L, plist.size(), 0);
	int newTable = lua_gettop(L);
	int index = 1;
	std::list<std::string>::const_iterator iter;
	for (iter = plist.begin(); iter != plist.end(); ++iter) {
		lua_pushstring(L, (*iter).c_str());
		lua_rawseti(L, newTable, index);
		index++;
	}
	return 1;
}

// show_formspec(formspec)
int ModApiClient::l_show_formspec(lua_State *L)
{
	if (!lua_isstring(L, 1) || !lua_isstring(L, 2))
		return 0;

	ClientEvent *event = new ClientEvent();
	event->type = CE_SHOW_LOCAL_FORMSPEC;
	event->show_formspec.formname = new std::string(luaL_checkstring(L, 1));
	event->show_formspec.formspec = new std::string(luaL_checkstring(L, 2));
	getClient(L)->pushToEventQueue(event);
	lua_pushboolean(L, true);
	return 1;
}

// send_respawn()
int ModApiClient::l_send_respawn(lua_State *L)
{
	getClient(L)->sendRespawn();
	return 0;
}

// disconnect()
int ModApiClient::l_disconnect(lua_State *L)
{
	// Stops badly written Lua code form causing boot loops
	if (getClient(L)->isShutdown()) {
		lua_pushboolean(L, false);
		return 1;
	}

	g_gamecallback->disconnect();
	lua_pushboolean(L, true);
	return 1;
}

// gettext(text)
int ModApiClient::l_gettext(lua_State *L)
{
	std::string text = strgettext(std::string(luaL_checkstring(L, 1)));
	lua_pushstring(L, text.c_str());

	return 1;
}

// get_node_or_nil(pos)
// pos = {x=num, y=num, z=num}
int ModApiClient::l_get_node_or_nil(lua_State *L)
{
	// pos
	v3s16 pos = read_v3s16(L, 1);

	// Do it
	bool pos_ok;
	MapNode n = getClient(L)->CSMGetNode(pos, &pos_ok);
	if (pos_ok) {
		// Return node
		pushnode(L, n, getClient(L)->ndef());
	} else {
		lua_pushnil(L);
	}
	return 1;
}

// get_langauge()
int ModApiClient::l_get_language(lua_State *L)
{
#ifdef _WIN32
	char *locale = setlocale(LC_ALL, NULL);
#else
	char *locale = setlocale(LC_MESSAGES, NULL);
#endif
	std::string lang = gettext("LANG_CODE");
	if (lang == "LANG_CODE")
		lang = "";

	lua_pushstring(L, locale);
	lua_pushstring(L, lang.c_str());
	return 2;
}

// get_meta(pos)
int ModApiClient::l_get_meta(lua_State *L)
{
	v3s16 p = read_v3s16(L, 1);

	// check restrictions first
	bool pos_ok;
	getClient(L)->CSMGetNode(p, &pos_ok);
	if (!pos_ok)
		return 0;

	NodeMetadata *meta = getEnv(L)->getMap().getNodeMetadata(p);
	NodeMetaRef::createClient(L, meta);
	return 1;
}

// sound_play(spec, parameters)
int ModApiClient::l_sound_play(lua_State *L)
{
	ISoundManager *sound = getClient(L)->getSoundManager();

	SimpleSoundSpec spec;
	read_soundspec(L, 1, spec);

	float gain = 1.0f;
	float pitch = 1.0f;
	bool looped = false;
	s32 handle;

	if (lua_istable(L, 2)) {
		getfloatfield(L, 2, "gain", gain);
		getfloatfield(L, 2, "pitch", pitch);
		getboolfield(L, 2, "loop", looped);

		lua_getfield(L, 2, "pos");
		if (!lua_isnil(L, -1)) {
			v3f pos = read_v3f(L, -1) * BS;
			lua_pop(L, 1);
			handle = sound->playSoundAt(
					spec.name, looped, gain * spec.gain, pos, pitch);
			lua_pushinteger(L, handle);
			return 1;
		}
	}

	handle = sound->playSound(spec.name, looped, gain * spec.gain, spec.fade, pitch);
	lua_pushinteger(L, handle);

	return 1;
}

// sound_stop(handle)
int ModApiClient::l_sound_stop(lua_State *L)
{
	s32 handle = luaL_checkinteger(L, 1);

	getClient(L)->getSoundManager()->stopSound(handle);

	return 0;
}

// sound_fade(handle, step, gain)
int ModApiClient::l_sound_fade(lua_State *L)
{
	s32 handle = luaL_checkinteger(L, 1);
	float step = readParam<float>(L, 2);
	float gain = readParam<float>(L, 3);
	getClient(L)->getSoundManager()->fadeSound(handle, step, gain);
	return 0;
}

// get_server_info()
int ModApiClient::l_get_server_info(lua_State *L)
{
	Client *client = getClient(L);
	Address serverAddress = client->getServerAddress();
	lua_newtable(L);
	lua_pushstring(L, client->getAddressName().c_str());
	lua_setfield(L, -2, "address");
	lua_pushstring(L, serverAddress.serializeString().c_str());
	lua_setfield(L, -2, "ip");
	lua_pushinteger(L, serverAddress.getPort());
	lua_setfield(L, -2, "port");
	lua_pushinteger(L, client->getProtoVersion());
	lua_setfield(L, -2, "protocol_version");
	return 1;
}

// get_item_def(itemstring)
int ModApiClient::l_get_item_def(lua_State *L)
{
	IGameDef *gdef = getGameDef(L);
	assert(gdef);

	IItemDefManager *idef = gdef->idef();
	assert(idef);

	if (checkCSMRestrictionFlag(CSM_RF_READ_ITEMDEFS))
		return 0;

	if (!lua_isstring(L, 1))
		return 0;

	std::string name = readParam<std::string>(L, 1);
	if (!idef->isKnown(name))
		return 0;
	const ItemDefinition &def = idef->get(name);

	push_item_definition_full(L, def);

	return 1;
}

// get_item_defs()
int ModApiClient::l_get_item_defs(lua_State *L)
{
	if (checkCSMRestrictionFlag(CSM_RF_READ_ITEMDEFS))
		return 0;

	IGameDef *gdef = getGameDef(L);
	assert(gdef);

	IItemDefManager *idef = gdef->idef();
	assert(idef);

	std::set<std::string> itemdefs;
	idef->getAll(itemdefs);
	lua_createtable(L, 0, itemdefs.size());
	for (const std::string &item_name : itemdefs) {
		const ItemDefinition &def = idef->get(item_name);
		lua_pushstring(L, item_name.c_str());
		push_item_definition_full(L, def);
		lua_settable(L, -3);
	}

	return 1;
}

// get_node_def(nodename)
int ModApiClient::l_get_node_def(lua_State *L)
{
	IGameDef *gdef = getGameDef(L);
	assert(gdef);

	const NodeDefManager *ndef = gdef->ndef();
	assert(ndef);

	if (!lua_isstring(L, 1))
		return 0;

	if (checkCSMRestrictionFlag(CSM_RF_READ_NODEDEFS))
		return 0;

	std::string name = readParam<std::string>(L, 1);
	const ContentFeatures &cf = ndef->get(ndef->getId(name));
	if (cf.name != name) // Unknown node. | name = <whatever>, cf.name = ignore
		return 0;

	push_content_features(L, cf);

	return 1;
}

// get_privilege_list()
int ModApiClient::l_get_privilege_list(lua_State *L)
{
	const Client *client = getClient(L);
	lua_newtable(L);
	for (const std::string &priv : client->getPrivilegeList()) {
		lua_pushboolean(L, true);
		lua_setfield(L, -2, priv.c_str());
	}
	return 1;
}

// get_builtin_path()
int ModApiClient::l_get_builtin_path(lua_State *L)
{
	lua_pushstring(L, BUILTIN_MOD_NAME ":");
	return 1;
}

// get_csm_restrictions()
int ModApiClient::l_get_csm_restrictions(lua_State *L)
{
	u64 flags = getClient(L)->getCSMRestrictionFlags();
	const CSMFlagDesc *flagdesc = flagdesc_csm_restriction;

	lua_newtable(L);
	for (int i = 0; flagdesc[i].name; i++) {
		setboolfield(L, -1, flagdesc[i].name, !!(flags & flagdesc[i].flag));
	}
	return 1;
}

// store_texture(texture, path)
int ModApiClient::l_store_texture(lua_State *L)
{
	const std::string texturename = luaL_checkstring(L, 1);
	const std::string filename = luaL_checkstring(L, 2);
	
	if (filename.empty())
		return 0;

	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_CURRENT_MOD_NAME);
	std::string current_mod = readParam<std::string>(L, -1, "");
	if (current_mod.empty()) {
		current_mod = getScriptApiBase(L)->getOrigin();
		if (current_mod.empty())
			return 0;
	}

	const ModSpec *mod = getClient(L)->getModSpec(current_mod);
	if (!mod)
		return 0;
	std::string base_path = fs::AbsolutePath(mod->path);
	std::string path = base_path + DIR_DELIM + filename;
	
	std::string abs_path = fs::AbsolutePath(path);
	if (abs_path.empty()) {
		std::string component;
		abs_path = fs::AbsolutePath(fs::RemoveLastPathComponent(path, &component));
		const char *supported_ext[] = {
			".png", ".jpg", ".bmp", ".tga",
			".pcx", ".ppm", ".psd", ".wal", ".rgb",
			NULL
		};
		if (abs_path.empty() || removeStringEnd(component, supported_ext).empty())
			return 0;
		abs_path += DIR_DELIM + component;
	}

	if (!fs::PathStartsWith(abs_path, base_path))
		return 0;

	video::ITexture* rt = getClient(L)->getTextureSource()->getTexture(texturename, nullptr);
	video::IVideoDriver *driver = RenderingEngine::get_video_driver();
	video::IImage* image = driver->createImageFromData(
		rt->getColorFormat(),
		rt->getSize(),
		rt->lock(),
		false 	// Copy memory
	);
	rt->unlock();
	if (!driver->writeImageToFile(image, abs_path.c_str(), 255.0))
		return 0;
	image->drop();
	lua_pushboolean(L, true);
	return 1;
}

void ModApiClient::Initialize(lua_State *L, int top)
{
	API_FCT(get_current_modname);
	API_FCT(get_modpath);
	API_FCT(print);
	API_FCT(display_chat_message);
	API_FCT(send_chat_message);
	API_FCT(clear_out_chat_queue);
	API_FCT(get_player_names);
	API_FCT(set_last_run_mod);
	API_FCT(get_last_run_mod);
	API_FCT(show_formspec);
	API_FCT(send_respawn);
	API_FCT(gettext);
	API_FCT(get_node_or_nil);
	API_FCT(disconnect);
	API_FCT(get_meta);
	API_FCT(sound_play);
	API_FCT(sound_stop);
	API_FCT(sound_fade);
	API_FCT(get_server_info);
	API_FCT(get_item_def);
	API_FCT(get_item_defs);
	API_FCT(get_node_def);
	API_FCT(get_privilege_list);
	API_FCT(get_builtin_path);
	API_FCT(get_language);
	API_FCT(get_csm_restrictions);
	API_FCT(store_texture);
}
