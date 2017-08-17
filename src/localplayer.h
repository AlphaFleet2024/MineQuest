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

#pragma once

#include "player.h"
#include "environment.h"
#include "constants.h"
#include <list>

class Client;
class Environment;
class GenericCAO;
class ClientActiveObject;
class ClientEnvironment;
class IGameDef;

enum LocalPlayerAnimations
{
	NO_ANIM,
	WALK_ANIM,
	DIG_ANIM,
	WD_ANIM
}; // no local animation, walking, digging, both

class LocalPlayer : public Player
{
public:
	LocalPlayer(Client *client, const char *name);
	virtual ~LocalPlayer() = default;

	ClientActiveObject *parent = nullptr;

	// Initialize hp to 0, so that no hearts will be shown if server
	// doesn't support health points
	u16 hp = 0;
	bool isAttached = false;
	bool touching_ground = false;
	// This oscillates so that the player jumps a bit above the surface
	bool in_liquid = false;
	// This is more stable and defines the maximum speed of the player
	bool in_liquid_stable = false;
	// Gets the viscosity of water to calculate friction
	u8 liquid_viscosity = 0;
	bool is_climbing = false;
	bool swimming_vertical = false;

	float physics_override_speed = 1.0f;
	float physics_override_jump = 1.0f;
	float physics_override_gravity = 1.0f;
	bool physics_override_sneak = true;
	bool physics_override_sneak_glitch = false;
	// Temporary option for old move code
	bool physics_override_new_move = true;

	v3f overridePosition;

	void move(f32 dtime, Environment *env, f32 pos_max_d);
	void move(f32 dtime, Environment *env, f32 pos_max_d,
			std::vector<CollisionInfo> *collision_info);
	// Temporary option for old move code
	void old_move(f32 dtime, Environment *env, f32 pos_max_d,
			std::vector<CollisionInfo> *collision_info);

	void applyControl(float dtime, Environment *env);

	v3s16 getStandingNodePos();
	v3s16 getFootstepNodePos();

	// Used to check if anything changed and prevent sending packets if not
	v3f last_position;
	v3f last_speed;
	float last_pitch = 0.0f;
	float last_yaw = 0.0f;
	unsigned int last_keyPressed = 0;
	u8 last_camera_fov = 0;
	u8 last_wanted_range = 0;

	float camera_impact = 0.0f;

	bool makes_footstep_sound = true;

	int last_animation = NO_ANIM;
	float last_animation_speed;

	std::string hotbar_image = "";
	std::string hotbar_selected_image = "";

	video::SColor light_color = video::SColor(255, 255, 255, 255);

	float hurt_tilt_timer = 0.0f;
	float hurt_tilt_strength = 0.0f;

	GenericCAO *getCAO() const { return m_cao; }

	void setCAO(GenericCAO *toset)
	{
		assert(!m_cao); // Pre-condition
		m_cao = toset;
	}

	u32 maxHudId() const { return hud.size(); }

	u16 getBreath() const { return m_breath; }
	void setBreath(u16 breath) { m_breath = breath; }

	v3s16 getLightPosition() const;

	void setYaw(f32 yaw) { m_yaw = yaw; }

	f32 getYaw() const { return m_yaw; }

	void setPitch(f32 pitch) { m_pitch = pitch; }

	f32 getPitch() const { return m_pitch; }

	inline void setPosition(const v3f &position)
	{
		m_position = position;
		m_sneak_node_exists = false;
	}

	v3f getPosition() const { return m_position; }
	v3f getEyePosition() const { return m_position + getEyeOffset(); }
	v3f getEyeOffset() const;

	void setCollisionbox(const aabb3f &box) { m_collisionbox = box; }

private:
	// clang-format off
	void accelerateHorizontal(const v3f &target_speed, f32 max_increase);
	// clang-format on
	void accelerateVertical(const v3f &target_speed, const f32 max_increase);
	bool updateSneakNode(Map *map, const v3f &position, const v3f &sneak_max);

	v3f m_position;

	v3s16 m_sneak_node = v3s16(32767, 32767, 32767);
	// Stores the top bounding box of m_sneak_node
	aabb3f m_sneak_node_bb_top = aabb3f(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	// Whether the player is allowed to sneak
	bool m_sneak_node_exists = false;
	// Whether a "sneak ladder" structure is detected at the players pos
	// see detectSneakLadder() in the .cpp for more info (always false if disabled)
	bool m_sneak_ladder_detected = false;

	// ***** Variables for temporary option of the old move code *****
	// Stores the max player uplift by m_sneak_node
	f32 m_sneak_node_bb_ymax = 0.0f;
	// Whether recalculation of m_sneak_node and its top bbox is needed
	bool m_need_to_get_new_sneak_node = true;
	// Node below player, used to determine whether it has been removed,
	// and its old type
	v3s16 m_old_node_below = v3s16(32767, 32767, 32767);
	std::string m_old_node_below_type = "air";
	// ***** End of variables for temporary option *****

	bool m_can_jump = false;
	u16 m_breath = PLAYER_MAX_BREATH;
	f32 m_yaw = 0.0f;
	f32 m_pitch = 0.0f;
	bool camera_barely_in_ceiling = false;
	aabb3f m_collisionbox = aabb3f(-BS * 0.30f, 0.0f, -BS * 0.30f, BS * 0.30f,
			BS * 1.75f, BS * 0.30f);

	GenericCAO *m_cao = nullptr;
	Client *m_client;
};
