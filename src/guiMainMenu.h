/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef GUIMAINMENU_HEADER
#define GUIMAINMENU_HEADER

#include "irrlichttypes_extrabloated.h"
#include "modalMenu.h"
#include <string>
#include <list>
#include "subgame.h"
#include "serverlist.h"

class IGameCallback;

enum {
	SERVERLIST_FAVORITES,
	SERVERLIST_PUBLIC,
};

struct MainMenuData
{
	// These are in the native format of the gui elements
	// Generic
	int selected_tab;
	std::string selected_game;
	std::string selected_game_name;
	// Client options
	std::string servername;
	std::string serverdescription;
	std::wstring address;
	std::wstring port;
	std::wstring name;
	std::wstring password;
	bool fancy_trees;
	bool smooth_lighting;
	bool clouds_3d;
	bool opaque_water;
	bool mip_map;
	bool anisotropic_filter;
	bool bilinear_filter;
	bool trilinear_filter;
	int enable_shaders;
	bool preload_item_visuals;
	bool enable_particles;
	bool liquid_finite;
	// Server options
	bool creative_mode;
	bool enable_damage;
	bool enable_public;
	int selected_world;
	bool simple_singleplayer_mode;
	// Actions
	std::wstring create_world_name;
	std::string create_world_gameid;
	bool only_refresh;

	int selected_serverlist;

	std::vector<WorldSpec> worlds;
	std::vector<SubgameSpec> games;
	std::vector<ServerListSpec> servers;

	MainMenuData():
		// Generic
		selected_tab(0),
		selected_game("minetest"),
		selected_game_name("Minetest"),
		// Client opts
		fancy_trees(false),
		smooth_lighting(false),
		// Server opts
		creative_mode(false),
		enable_damage(false),
		enable_public(false),
		selected_world(0),
		simple_singleplayer_mode(false),
		// Actions
		only_refresh(false),

		selected_serverlist(SERVERLIST_FAVORITES)
	{}
};

class GUIMainMenu : public GUIModalMenu
{
public:
	GUIMainMenu(gui::IGUIEnvironment* env,
			gui::IGUIElement* parent, s32 id,
			IMenuManager *menumgr,
			MainMenuData *data,
			IGameCallback *gamecallback);
	~GUIMainMenu();
	
	void removeChildren();
	// Remove and re-add (or reposition) stuff
	void regenerateGui(v2u32 screensize);
	void drawMenu();
	void readInput(MainMenuData *dst);
	void acceptInput();
	bool getStatus()
	{ return m_accepted; }
	bool OnEvent(const SEvent& event);
	void createNewWorld(std::wstring name, std::string gameid);
	void deleteWorld(const std::vector<std::string> &paths);
	int getTab();
	void displayMessageMenu(std::wstring msg);
	
private:
	MainMenuData *m_data;
	bool m_accepted;
	IGameCallback *m_gamecallback;

	gui::IGUIEnvironment* env;
	gui::IGUIElement* parent;
	s32 id;
	IMenuManager *menumgr;

	std::vector<int> m_world_indices;

	bool m_is_regenerating;
	v2s32 m_topleft_client;
	v2s32 m_size_client;
	v2s32 m_topleft_server;
	v2s32 m_size_server;
	void updateGuiServerList();
	void serverListOnSelected();
	ServerListSpec getServerListSpec(std::string address, std::string port);
};

#endif

