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

#pragma once

#include <iostream>
#include <vector>
#include "irrlichttypes_extrabloated.h"
#include "irr_ptr.h"
#include "../particles.h"

struct ClientEvent;
class ParticleManager;
class ClientEnvironment;
struct MapNode;
struct ContentFeatures;
class LocalPlayer;
class ITextureSource;
class IGameDef;
class Client;

struct ClientParticleTexture
{
	/* per-spawner structure used to store the ParticleTexture structs
	 * that spawned particles will refer to through ClientParticleTexRef */
	ParticleTexture tex;
	video::ITexture *ref = nullptr;

	ClientParticleTexture() = default;
	ClientParticleTexture(const ServerParticleTexture& p, ITextureSource *tsrc);
	ClientParticleTexture(const ParticleTexture& p, video::ITexture *t):
			tex(p),
			ref(t) {}
};

struct ClientParticleTexRef
{
	/* per-particle structure used to avoid massively duplicating the
	 * fairly large ParticleTexture struct */
	ParticleTexture *tex = nullptr;
	video::ITexture *ref = nullptr;

	ClientParticleTexRef() = default;

	/* constructor used by particles spawned from a spawner */
	explicit ClientParticleTexRef(ClientParticleTexture &t):
			tex(&t.tex), ref(t.ref) {};

	/* constructor used for node particles */
	explicit ClientParticleTexRef(video::ITexture *tp): ref(tp) {};
};

class ParticleSpawner;
class ParticleBuffer;

class Particle
{
public:
	Particle(
		IGameDef *gamedef,
		LocalPlayer *player,
		ClientEnvironment *env,
		const ParticleParameters &p,
		const ClientParticleTexRef &texture,
		v2f texpos,
		v2f texsize,
		video::SColor color,
		ParticleSpawner *parent = nullptr,
		std::unique_ptr<ClientParticleTexture> owned_texture = nullptr
	);

	~Particle();

	DISABLE_CLASS_COPY(Particle)

	void step(float dtime);

	bool isExpired ()
	{ return m_expiration < m_time; }

	ParticleSpawner *getParent() { return m_parent; }

	const ClientParticleTexRef &getTextureRef() const { return m_texture; }
	bool attachToBuffer(ParticleBuffer *buffer);

private:
	void updateLight();
	void updateVertices();

	ParticleBuffer *m_buffer = nullptr;
	u16 m_index; // index in m_buffer

	float m_time = 0.0f;
	float m_expiration;

	ClientEnvironment *m_env;
	IGameDef *m_gamedef;
	aabb3f m_collisionbox;
	ClientParticleTexRef m_texture;
	v2f m_texpos;
	v2f m_texsize;
	v3f m_pos;
	v3f m_velocity;
	v3f m_acceleration;
	const ParticleParameters m_p;
	LocalPlayer *m_player;

	//! Color without lighting
	video::SColor m_base_color;
	//! Final rendered color
	video::SColor m_color;
	float m_animation_time = 0.0f;
	int m_animation_frame = 0;
	float m_alpha = 0.0f;

	ParticleSpawner *m_parent = nullptr;
	// Used if not spawned from a particlespawner
	std::unique_ptr<ClientParticleTexture> m_owned_texture;
};

class ParticleSpawner
{
public:
	ParticleSpawner(IGameDef *gamedef,
		LocalPlayer *player,
		const ParticleSpawnerParameters &params,
		u16 attached_id,
		std::vector<ClientParticleTexture> &&texpool,
		ParticleManager *p_manager);

	void step(float dtime, ClientEnvironment *env);

	bool getExpired() const
	{ return p.amount <= 0 && p.time != 0; }

	bool hasActive() const { return m_active != 0; }
	void decrActive() { m_active -= 1; }

private:
	void spawnParticle(ClientEnvironment *env, float radius,
		const core::matrix4 *attached_absolute_pos_rot_matrix);

	size_t m_active;
	ParticleManager *m_particlemanager;
	float m_time;
	IGameDef *m_gamedef;
	LocalPlayer *m_player;
	ParticleSpawnerParameters p;
	std::vector<ClientParticleTexture> m_texpool;
	std::vector<float> m_spawntimes;
	u16 m_attached_id;
};

class ParticleBuffer : public scene::ISceneNode
{
public:
	ParticleBuffer(ClientEnvironment *env, const video::SMaterial &material);

	// for pointer stability
	DISABLE_CLASS_COPY(ParticleBuffer)

	std::optional<u16> allocate();
	void release(u16 index);

	// video::S3DVertex[4]
	video::S3DVertex *getVertices(u16 index);

	inline bool isEmpty() const {
		return m_free_list.size() == m_count;
	}

	// usage timer is reset when a particle is added
	inline float getUsageTimer() const {
		return m_usage_timer;
	}
	inline void increaseUsageTimer(float dtime) {
		m_usage_timer += dtime;
	}

	virtual video::SMaterial &getMaterial(u32 num) override {
		return m_mesh_buffer->getMaterial();
	}
	virtual u32 getMaterialCount() const override {
		return 1;
	}

	virtual const core::aabbox3df &getBoundingBox() const override;

	virtual void render() override;

	virtual void OnRegisterSceneNode() override;

	// we have 16-bit indices
	static constexpr u16 MAX_PARTICLES_PER_BUFFER = 16000;

private:
	irr_ptr<scene::SMeshBuffer> m_mesh_buffer;
	std::vector<u16> m_free_list;
	float m_usage_timer = 0;
	u16 m_count = 0;
	mutable bool m_bounding_box_dirty = true;
};

/**
 * Class doing particle as well as their spawners handling
 */
class ParticleManager
{
friend class ParticleSpawner;
public:
	ParticleManager(ClientEnvironment* env);
	DISABLE_CLASS_COPY(ParticleManager)
	~ParticleManager();

	void step (float dtime);

	void handleParticleEvent(ClientEvent *event, Client *client,
			LocalPlayer *player);

	void addDiggingParticles(IGameDef *gamedef, LocalPlayer *player, v3s16 pos,
		const MapNode &n, const ContentFeatures &f);

	void addNodeParticle(IGameDef *gamedef, LocalPlayer *player, v3s16 pos,
		const MapNode &n, const ContentFeatures &f);

	void reserveParticleSpace(size_t max_estimate);

	/**
	 * This function is only used by client particle spawners
	 *
	 * We don't need to check the particle spawner list because client ID will
	 * never overlap (u64)
	 * @return new id
	 */
	u64 generateSpawnerId()
	{
		return m_next_particle_spawner_id++;
	}

protected:
	static bool getNodeParticleParams(const MapNode &n, const ContentFeatures &f,
		ParticleParameters &p, video::ITexture **texture, v2f &texpos,
		v2f &texsize, video::SColor *color, u8 tilenum = 0);

	static video::SMaterial getMaterialForParticle(const ClientParticleTexRef &texture);

	bool addParticle(std::unique_ptr<Particle> toadd);

private:
	void addParticleSpawner(u64 id, std::unique_ptr<ParticleSpawner> toadd);
	void deleteParticleSpawner(u64 id);

	void stepParticles(float dtime);
	void stepSpawners(float dtime);
	void stepBuffers(float dtime);

	void clearAll();

	std::vector<std::unique_ptr<Particle>> m_particles;
	std::unordered_map<u64, std::unique_ptr<ParticleSpawner>> m_particle_spawners;
	std::vector<std::unique_ptr<ParticleSpawner>> m_dying_particle_spawners;
	std::vector<irr_ptr<ParticleBuffer>> m_particle_buffers;

	// Start the particle spawner ids generated from here after u32_max.
	// lower values are for server sent spawners.
	u64 m_next_particle_spawner_id = static_cast<u64>(U32_MAX) + 1;

	ClientEnvironment *m_env;

	IntervalLimiter m_buffer_gc;

	std::mutex m_particle_list_lock;
	std::mutex m_spawner_list_lock;
};
