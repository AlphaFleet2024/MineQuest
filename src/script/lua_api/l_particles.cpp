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

#include "lua_api/l_particles.h"
#include "lua_api/l_object.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_particleparams.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "server.h"
#include "particles.h"

// add_particle({pos=, velocity=, acceleration=, expirationtime=,
//     size=, collisiondetection=, collision_removal=, object_collision=,
//     vertical=, texture=, player=})
// pos/velocity/acceleration = {x=num, y=num, z=num}
// expirationtime = num (seconds)
// size = num
// collisiondetection = bool
// collision_removal = bool
// object_collision = bool
// vertical = bool
// texture = e.g."default_wood.png"
// animation = TileAnimation definition
// glow = num

void LuaParticleParams::readTexValue(lua_State* L, ServerParticleTexture& tex) {
	StackUnroller unroll(L);
	tex.animated = false;
	if (lua_isstring(L, -1)) {
		tex.string = lua_tostring(L, -1);
	} else {
		luaL_checktype(L, -1, LUA_TTABLE);
		lua_getfield(L, -1, "name");
		tex.string = luaL_checkstring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "animation");
		if (! lua_isnil(L, -1)) {
			tex.animated = true;
			tex.animation = read_animation_definition(L, -1);
		}
		lua_pop(L, 1);

		LuaParticleParams::readTweenTable(L, "alpha", tex.alpha);
		LuaParticleParams::readTweenTable(L, "scale", tex.scale);
	}
}

int ModApiParticles::l_add_particle(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	// Get parameters
	struct ParticleParameters p;
	std::string playername;

	if (lua_gettop(L) > 1) // deprecated
	{
		log_deprecated(L, "Deprecated add_particle call with "
			"individual parameters instead of definition");
		p.pos = check_v3f(L, 1);
		p.vel = check_v3f(L, 2);
		p.acc = check_v3f(L, 3);
		p.expirationtime = luaL_checknumber(L, 4);
		p.size = luaL_checknumber(L, 5);
		p.collisiondetection = readParam<bool>(L, 6);
		p.texture.string = luaL_checkstring(L, 7);
		if (lua_gettop(L) == 8) // only spawn for a single player
			playername = luaL_checkstring(L, 8);
	}
	else if (lua_istable(L, 1))
	{
		lua_getfield(L, 1, "pos");
		if (lua_istable(L, -1))
			p.pos = check_v3f(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "vel");
		if (lua_istable(L, -1)) {
			p.vel = check_v3f(L, -1);
			log_deprecated(L, "The use of vel is deprecated. "
				"Use velocity instead");
		}
		lua_pop(L, 1);

		lua_getfield(L, 1, "velocity");
		if (lua_istable(L, -1))
			p.vel = check_v3f(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "acc");
		if (lua_istable(L, -1)) {
			p.acc = check_v3f(L, -1);
			log_deprecated(L, "The use of acc is deprecated. "
				"Use acceleration instead");
		}
		lua_pop(L, 1);

		lua_getfield(L, 1, "acceleration");
		if (lua_istable(L, -1))
			p.acc = check_v3f(L, -1);
		lua_pop(L, 1);

		p.expirationtime = getfloatfield_default(L, 1, "expirationtime",
			p.expirationtime);
		p.size = getfloatfield_default(L, 1, "size", p.size);
		p.collisiondetection = getboolfield_default(L, 1,
			"collisiondetection", p.collisiondetection);
		p.collision_removal = getboolfield_default(L, 1,
			"collision_removal", p.collision_removal);
		p.object_collision = getboolfield_default(L, 1,
			"object_collision", p.object_collision);
		p.vertical = getboolfield_default(L, 1, "vertical", p.vertical);

		lua_getfield(L, 1, "animation");
		p.animation = read_animation_definition(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "texture");
		if (! lua_isnil(L, -1)) {
			LuaParticleParams::readTexValue(L, p.texture);
		}
		lua_pop(L, 1);

		p.glow = getintfield_default(L, 1, "glow", p.glow);

		lua_getfield(L, 1, "node");
		if (lua_istable(L, -1))
			p.node = readnode(L, -1, getGameDef(L)->ndef());
		lua_pop(L, 1);

		p.node_tile = getintfield_default(L, 1, "node_tile", p.node_tile);

		playername = getstringfield_default(L, 1, "playername", "");

		lua_getfield(L, 1, "drag");
		if (lua_istable(L, -1))
			p.drag = check_v3f(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "bounce");
		LuaParticleParams::readLuaValue(L, p.bounce);
		lua_pop(L, 1);
	}

	getServer(L)->spawnParticle(playername, p);
	return 1;
}

// add_particlespawner({amount=, time=,
//				minpos=, maxpos=,
//				minvel=, maxvel=,
//				minacc=, maxacc=,
//				minexptime=, maxexptime=,
//				minsize=, maxsize=,
//				collisiondetection=,
//				collision_removal=,
//				object_collision=,
//				vertical=,
//				texture=,
//				player=})
// minpos/maxpos/minvel/maxvel/minacc/maxacc = {x=num, y=num, z=num}
// minexptime/maxexptime = num (seconds)
// minsize/maxsize = num
// collisiondetection = bool
// collision_removal = bool
// object_collision = bool
// vertical = bool
// texture = e.g."default_wood.png"
// animation = TileAnimation definition
// glow = num

int ModApiParticles::l_add_particlespawner(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	// Get parameters
	ParticleSpawnerParameters p;
	ServerActiveObject *attached = NULL;
	std::string playername;

	using namespace ParticleParamTypes;
	if (lua_gettop(L) > 1) //deprecated
	{
		log_deprecated(L, "Deprecated add_particlespawner call with "
			"individual parameters instead of definition");
		p.amount = luaL_checknumber(L, 1);
		p.time = luaL_checknumber(L, 2);
		auto minpos = check_v3f(L, 3);
		auto maxpos = check_v3f(L, 4);
		auto minvel = check_v3f(L, 5);
		auto maxvel = check_v3f(L, 6);
		auto minacc = check_v3f(L, 7);
		auto maxacc = check_v3f(L, 8);
		auto minexptime = luaL_checknumber(L, 9);
		auto maxexptime = luaL_checknumber(L, 10);
		auto minsize = luaL_checknumber(L, 11);
		auto maxsize = luaL_checknumber(L, 12);
		p.pos = v3fRange(minpos, maxpos);
		p.vel = v3fRange(minvel, maxvel);
		p.acc = v3fRange(minacc, maxacc);
		p.exptime = f32Range(minexptime, maxexptime);
		p.size = f32Range(minsize, maxsize);

		p.collisiondetection = readParam<bool>(L, 13);
		p.texture.string = luaL_checkstring(L, 14);
		if (lua_gettop(L) == 15) // only spawn for a single player
			playername = luaL_checkstring(L, 15);
	}
	else if (lua_istable(L, 1))
	{
		p.amount = getintfield_default(L, 1, "amount", p.amount);
		p.time = getfloatfield_default(L, 1, "time", p.time);

		// set default values
		p.exptime = 1;
		p.size = 1;

		// read spawner parameters from the table
		LuaParticleParams::readTweenTable(L, "pos", p.pos);
		LuaParticleParams::readTweenTable(L, "vel", p.vel);
		LuaParticleParams::readTweenTable(L, "acc", p.acc);
		LuaParticleParams::readTweenTable(L, "size", p.size);
		LuaParticleParams::readTweenTable(L, "exptime", p.exptime);
		LuaParticleParams::readTweenTable(L, "drag", p.drag);
		LuaParticleParams::readTweenTable(L, "bounce", p.bounce);
		lua_getfield(L, 1, "attract");
		if (!lua_isnil(L, -1)) {
			luaL_checktype(L, -1, LUA_TTABLE);
			lua_getfield(L, -1, "kind");
			LuaParticleParams::readLuaValue(L, p.attractor_kind);
			lua_pop(L,1);
			lua_getfield(L, -1, "die_on_contact");
			if (!lua_isnil(L,-1)) {
				bool v = lua_toboolean(L, -1);
				p.attractor_kill = v;
			}
			lua_pop(L,1);

			if (p.attractor_kind != ParticleParamTypes::AttractorKind::none) {
				LuaParticleParams::readTweenTable(L, "strength", p.attract);
				LuaParticleParams::readTweenTable(L, "origin", p.attractor);
				p.attractor_attachment = LuaParticleParams::readAttachmentID(L, "origin_attached");
				if (p.attractor_kind != ParticleParamTypes::AttractorKind::point) {
					LuaParticleParams::readTweenTable(L, "angle", p.attractor_angle);
					p.attractor_angle_attachment = LuaParticleParams::readAttachmentID(L, "angle_attached");
				}
			}
		} else {
			p.attractor_kind = ParticleParamTypes::AttractorKind::none;
		}
// 		using ParticleParamTypes::dump;
// 		std::cerr<<((u16)p.attractor_kind)<<" :: attractor strength " << (f32)p.attract.start.min <<":" << dump(p.attract)<< " / att origin " << dump(p.attractor) << " / angle " << dump(p.attractor_angle)<<"\n";
		lua_pop(L,1);
		LuaParticleParams::readTweenTable(L, "radius", p.radius);

		p.collisiondetection = getboolfield_default(L, 1,
			"collisiondetection", p.collisiondetection);
		p.collision_removal = getboolfield_default(L, 1,
			"collision_removal", p.collision_removal);
		p.object_collision = getboolfield_default(L, 1,
			"object_collision", p.object_collision);

		lua_getfield(L, 1, "animation");
		p.animation = read_animation_definition(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "attached");
		if (!lua_isnil(L, -1)) {
			ObjectRef *ref = ObjectRef::checkobject(L, -1);
			lua_pop(L, 1);
			attached = ObjectRef::getobject(ref);
		}

		lua_getfield(L, 1, "texture");
		if (not lua_isnil(L, -1)) {
			LuaParticleParams::readTexValue(L, p.texture);
		}
		lua_pop(L, 1);

		p.vertical = getboolfield_default(L, 1, "vertical", p.vertical);
		playername = getstringfield_default(L, 1, "playername", "");
		p.glow = getintfield_default(L, 1, "glow", p.glow);

		lua_getfield(L, 1, "texpool");
		if (lua_istable(L, -1)) {
			size_t tl = lua_objlen(L, -1);
			p.texpool.reserve(tl);
			for (size_t i = 0; i < tl; ++i) {
				lua_pushinteger(L, i+1), lua_gettable(L, -2);
				p.texpool.emplace_back();
				LuaParticleParams::readTexValue(L, p.texpool.back());
				lua_pop(L,1);
			}
		}
		lua_pop(L, 1);

		lua_getfield(L, 1, "node");
		if (lua_istable(L, -1))
			p.node = readnode(L, -1, getGameDef(L)->ndef());
		lua_pop(L, 1);

		p.node_tile = getintfield_default(L, 1, "node_tile", p.node_tile);
	}

	u32 id = getServer(L)->addParticleSpawner(p, attached, playername);
	lua_pushnumber(L, id);

	return 1;
}

// delete_particlespawner(id, player)
// player (string) is optional
int ModApiParticles::l_delete_particlespawner(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	// Get parameters
	u32 id = luaL_checknumber(L, 1);
	std::string playername;
	if (lua_gettop(L) == 2) {
		playername = luaL_checkstring(L, 2);
	}

	getServer(L)->deleteParticleSpawner(playername, id);
	return 1;
}

void ModApiParticles::Initialize(lua_State *L, int top)
{
	API_FCT(add_particle);
	API_FCT(add_particlespawner);
	API_FCT(delete_particlespawner);
}

