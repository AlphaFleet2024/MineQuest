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

#include "map.h"
#include "content/subgames.h"
#include "mapsector.h"
#include "mapblock.h"
#include "filesys.h"
#include "voxel.h"
#include "voxelalgorithms.h"
#include "porting.h"
#include "serialization.h"
#include "nodemetadata.h"
#include "settings.h"
#include "log.h"
#include "profiler.h"
#include "nodedef.h"
#include "gamedef.h"
#include "util/directiontables.h"
#include "rollback_interface.h"
#include "environment.h"
#include "reflowscan.h"
#include "emerge.h"
#include "mapgen/mapgen_v6.h"
#include "mapgen/mg_biome.h"
#include "config.h"
#include "server.h"
#include "database/database.h"
#include "database/database-dummy.h"
#include "database/database-sqlite3.h"
#include "script/scripting_server.h"
#include "irrlicht_changes/printing.h"
#include <deque>
#include <queue>
#if USE_LEVELDB
#include "database/database-leveldb.h"
#endif
#if USE_REDIS
#include "database/database-redis.h"
#endif
#if USE_POSTGRESQL
#include "database/database-postgresql.h"
#endif


/*
	Map
*/

Map::Map(IGameDef *gamedef):
	m_gamedef(gamedef),
	m_nodedef(gamedef->ndef())
{
}

Map::~Map()
{
	/*
		Free all MapSectors
	*/
	for (auto &sector : m_sectors) {
		delete sector.second;
	}
}

void Map::addEventReceiver(MapEventReceiver *event_receiver)
{
	m_event_receivers.insert(event_receiver);
}

void Map::removeEventReceiver(MapEventReceiver *event_receiver)
{
	m_event_receivers.erase(event_receiver);
}

void Map::dispatchEvent(const MapEditEvent &event)
{
	for (MapEventReceiver *event_receiver : m_event_receivers) {
		event_receiver->onMapEditEvent(event);
	}
}

MapSector * Map::getSectorNoGenerateNoLock(v2s16 p)
{
	if(m_sector_cache != NULL && p == m_sector_cache_p){
		MapSector * sector = m_sector_cache;
		return sector;
	}

	auto n = m_sectors.find(p);

	if (n == m_sectors.end())
		return NULL;

	MapSector *sector = n->second;

	// Cache the last result
	m_sector_cache_p = p;
	m_sector_cache = sector;

	return sector;
}

MapSector *Map::getSectorNoGenerate(v2s16 p)
{
	return getSectorNoGenerateNoLock(p);
}

MapBlock *Map::getBlockNoCreateNoEx(v3s16 p3d)
{
	v2s16 p2d(p3d.X, p3d.Z);
	MapSector *sector = getSectorNoGenerate(p2d);
	if (!sector)
		return nullptr;
	MapBlock *block = sector->getBlockNoCreateNoEx(p3d.Y);
	return block;
}

MapBlock *Map::getBlockNoCreate(v3s16 p3d)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d);
	if(block == NULL)
		throw InvalidPositionException();
	return block;
}

bool Map::isValidPosition(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	return (block != NULL);
}

// Returns a CONTENT_IGNORE node if not found
MapNode Map::getNode(v3s16 p, bool *is_valid_position)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (block == NULL) {
		if (is_valid_position != NULL)
			*is_valid_position = false;
		return {CONTENT_IGNORE};
	}

	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	MapNode node = block->getNodeNoCheck(relpos);
	if (is_valid_position != NULL)
		*is_valid_position = true;
	return node;
}

static void set_node_in_block(MapBlock *block, v3s16 relpos, MapNode n)
{
	// Never allow placing CONTENT_IGNORE, it causes problems
	if(n.getContent() == CONTENT_IGNORE){
		const NodeDefManager *nodedef = block->getParent()->getNodeDefManager();
		v3s16 blockpos = block->getPos();
		v3s16 p = blockpos * MAP_BLOCKSIZE + relpos;
		errorstream<<"Not allowing to place CONTENT_IGNORE"
				<<" while trying to replace \""
				<<nodedef->get(block->getNodeNoCheck(relpos)).name
				<<"\" at "<<p<<" (block "<<blockpos<<")"<<std::endl;
		return;
	}
	block->setNodeNoCheck(relpos, n);
}

// throws InvalidPositionException if not found
void Map::setNode(v3s16 p, MapNode n)
{
	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreate(blockpos);
	v3s16 relpos = p - blockpos*MAP_BLOCKSIZE;
	set_node_in_block(block, relpos, n);
}

void Map::addNodeAndUpdate(v3s16 p, MapNode n,
		std::map<v3s16, MapBlock*> &modified_blocks,
		bool remove_metadata)
{
	// Collect old node for rollback
	RollbackNode rollback_oldnode(this, p, m_gamedef);

	v3s16 blockpos = getNodeBlockPos(p);
	MapBlock *block = getBlockNoCreate(blockpos);
	v3s16 relpos = p - blockpos * MAP_BLOCKSIZE;

	// This is needed for updating the lighting
	MapNode oldnode = block->getNodeNoCheck(relpos);

	// Remove node metadata
	if (remove_metadata) {
		removeNodeMetadata(p);
	}

	// Set the node on the map
	ContentLightingFlags f = m_nodedef->getLightingFlags(n);
	ContentLightingFlags oldf = m_nodedef->getLightingFlags(oldnode);
	if (f == oldf) {
		// No light update needed, just copy over the old light.
		n.setLight(LIGHTBANK_DAY, oldnode.getLightRaw(LIGHTBANK_DAY, oldf), f);
		n.setLight(LIGHTBANK_NIGHT, oldnode.getLightRaw(LIGHTBANK_NIGHT, oldf), f);
		set_node_in_block(block, relpos, n);

		modified_blocks[blockpos] = block;
	} else {
		// Ignore light (because calling voxalgo::update_lighting_nodes)
		n.setLight(LIGHTBANK_DAY, 0, f);
		n.setLight(LIGHTBANK_NIGHT, 0, f);
		set_node_in_block(block, relpos, n);

		// Update lighting
		std::vector<std::pair<v3s16, MapNode> > oldnodes;
		oldnodes.emplace_back(p, oldnode);
		voxalgo::update_lighting_nodes(this, oldnodes, modified_blocks);

		for (auto &modified_block : modified_blocks) {
			modified_block.second->expireDayNightDiff();
		}
	}

	// Report for rollback
	if(m_gamedef->rollback())
	{
		RollbackNode rollback_newnode(this, p, m_gamedef);
		RollbackAction action;
		action.setSetNode(p, rollback_oldnode, rollback_newnode);
		m_gamedef->rollback()->reportAction(action);
	}
}

void Map::removeNodeAndUpdate(v3s16 p,
		std::map<v3s16, MapBlock*> &modified_blocks)
{
	addNodeAndUpdate(p, MapNode(CONTENT_AIR), modified_blocks, true);
}

bool Map::addNodeWithEvent(v3s16 p, MapNode n, bool remove_metadata)
{
	MapEditEvent event;
	event.type = remove_metadata ? MEET_ADDNODE : MEET_SWAPNODE;
	event.p = p;
	event.n = n;

	bool succeeded = true;
	try{
		std::map<v3s16, MapBlock*> modified_blocks;
		addNodeAndUpdate(p, n, modified_blocks, remove_metadata);

		event.setModifiedBlocks(modified_blocks);
	}
	catch(InvalidPositionException &e){
		succeeded = false;
	}

	dispatchEvent(event);

	return succeeded;
}

bool Map::removeNodeWithEvent(v3s16 p)
{
	MapEditEvent event;
	event.type = MEET_REMOVENODE;
	event.p = p;

	bool succeeded = true;
	try{
		std::map<v3s16, MapBlock*> modified_blocks;
		removeNodeAndUpdate(p, modified_blocks);

		event.setModifiedBlocks(modified_blocks);
	}
	catch(InvalidPositionException &e){
		succeeded = false;
	}

	dispatchEvent(event);

	return succeeded;
}

struct TimeOrderedMapBlock {
	MapSector *sect;
	MapBlock *block;

	TimeOrderedMapBlock(MapSector *sect, MapBlock *block) :
		sect(sect),
		block(block)
	{}

	bool operator<(const TimeOrderedMapBlock &b) const
	{
		return block->getUsageTimer() < b.block->getUsageTimer();
	};
};

/*
	Updates usage timers
*/
void Map::timerUpdate(float dtime, float unload_timeout, s32 max_loaded_blocks,
		std::vector<v3s16> *unloaded_blocks)
{
	bool save_before_unloading = maySaveBlocks();

	// Profile modified reasons
	Profiler modprofiler;

	std::vector<v2s16> sector_deletion_queue;
	u32 deleted_blocks_count = 0;
	u32 saved_blocks_count = 0;
	u32 block_count_all = 0;
	u32 locked_blocks = 0;

	const auto start_time = porting::getTimeUs();
	beginSave();

	// If there is no practical limit, we spare creation of mapblock_queue
	if (max_loaded_blocks < 0) {
		for (auto &sector_it : m_sectors) {
			MapSector *sector = sector_it.second;

			bool all_blocks_deleted = true;

			MapBlockVect blocks;
			sector->getBlocks(blocks);

			for (MapBlock *block : blocks) {
				block->incrementUsageTimer(dtime);

				if (block->refGet() == 0
						&& block->getUsageTimer() > unload_timeout) {
					v3s16 p = block->getPos();

					// Save if modified
					if (block->getModified() != MOD_STATE_CLEAN
							&& save_before_unloading) {
						modprofiler.add(block->getModifiedReasonString(), 1);
						if (!saveBlock(block))
							continue;
						saved_blocks_count++;
					}

					// Delete from memory
					sector->deleteBlock(block);

					if (unloaded_blocks)
						unloaded_blocks->push_back(p);

					deleted_blocks_count++;
				} else {
					all_blocks_deleted = false;
					block_count_all++;
				}
			}

			// Delete sector if we emptied it
			if (all_blocks_deleted) {
				sector_deletion_queue.push_back(sector_it.first);
			}
		}
	} else {
		std::priority_queue<TimeOrderedMapBlock> mapblock_queue;
		for (auto &sector_it : m_sectors) {
			MapSector *sector = sector_it.second;

			MapBlockVect blocks;
			sector->getBlocks(blocks);

			for (MapBlock *block : blocks) {
				block->incrementUsageTimer(dtime);
				mapblock_queue.push(TimeOrderedMapBlock(sector, block));
			}
		}
		block_count_all = mapblock_queue.size();

		// Delete old blocks, and blocks over the limit from the memory
		while (!mapblock_queue.empty() && ((s32)mapblock_queue.size() > max_loaded_blocks
				|| mapblock_queue.top().block->getUsageTimer() > unload_timeout)) {
			TimeOrderedMapBlock b = mapblock_queue.top();
			mapblock_queue.pop();

			MapBlock *block = b.block;

			if (block->refGet() != 0) {
				locked_blocks++;
				continue;
			}

			v3s16 p = block->getPos();

			// Save if modified
			if (block->getModified() != MOD_STATE_CLEAN && save_before_unloading) {
				modprofiler.add(block->getModifiedReasonString(), 1);
				if (!saveBlock(block))
					continue;
				saved_blocks_count++;
			}

			// Delete from memory
			b.sect->deleteBlock(block);

			if (unloaded_blocks)
				unloaded_blocks->push_back(p);

			deleted_blocks_count++;
			block_count_all--;
		}

		// Delete empty sectors
		for (auto &sector_it : m_sectors) {
			if (sector_it.second->empty()) {
				sector_deletion_queue.push_back(sector_it.first);
			}
		}
	}

	endSave();
	const auto end_time = porting::getTimeUs();

	reportMetrics(end_time - start_time, saved_blocks_count, block_count_all);

	// Finally delete the empty sectors
	deleteSectors(sector_deletion_queue);

	if(deleted_blocks_count != 0)
	{
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Unloaded "<<deleted_blocks_count
				<<" blocks from memory";
		if(save_before_unloading)
			infostream<<", of which "<<saved_blocks_count<<" were written";
		infostream<<", "<<block_count_all<<" blocks in memory, " << locked_blocks << " locked";
		infostream<<"."<<std::endl;
		if(saved_blocks_count != 0){
			PrintInfo(infostream); // ServerMap/ClientMap:
			infostream<<"Blocks modified by: "<<std::endl;
			modprofiler.print(infostream);
		}
	}
}

void Map::unloadUnreferencedBlocks(std::vector<v3s16> *unloaded_blocks)
{
	timerUpdate(0.0, -1.0, 0, unloaded_blocks);
}

void Map::deleteSectors(std::vector<v2s16> &sectorList)
{
	for (v2s16 j : sectorList) {
		MapSector *sector = m_sectors[j];
		// If sector is in sector cache, remove it from there
		if(m_sector_cache == sector)
			m_sector_cache = NULL;
		// Remove from map and delete
		m_sectors.erase(j);
		delete sector;
	}
}

void Map::PrintInfo(std::ostream &out)
{
	out<<"Map: ";
}

#define WATER_DROP_BOOST 4

const static v3s16 liquid_7dirs[7] = {
	v3s16( 0, 0, 0),
	// order: upper before same level before lower
	v3s16( 0, 1, 0),
	v3s16( 0, 0, 1),
	v3s16( 1, 0, 0),
	v3s16( 0, 0,-1),
	v3s16(-1, 0, 0),
	v3s16( 0,-1, 0)
};

void ServerMap::transforming_liquid_add(v3s16 p) {
		m_transforming_liquid.push_back(p);
}


class LiquidSystem {
	public:
	LiquidSystem(ServerMap *map) : m_map(map)
	{
		m_nodedef = map->getNodeDefManager();
	}

	void enterNode(const v3s16& p0, UniqueQueue<v3s16>& transforming_liquid)
	{
		for(u16 i = 0; i < CNT_DIRS; ++ i) p[i] = p0 + liquid_7dirs[i];
		for(u16 i = 0; i < CNT_DIRS; ++ i) n[i] = m_map->getNode(p[i]);
		for(u16 i = 0; i < CNT_DIRS; ++ i) n_old[i] = n[i];
		for(u16 i = 0; i < CNT_DIRS; ++ i) d[i] = &m_nodedef->get(n[i]);
		for(u16 i = 0; i < CNT_DIRS; ++ i) d_old[i] = d[i];

		do {
			if(handleRenewableLiquid(transforming_liquid)) break;
			if(handleSinkingLiquid(transforming_liquid))   break;
			if(handleRemovedLiquid(transforming_liquid))   break;
			if(handleViscosityLiquid(transforming_liquid)) break;
			if(handleFlowDownLiquid(transforming_liquid))  break;
			if(handleSpreadingLiquid(transforming_liquid)) break;
		} while(0);
	}

	void writeChangedNodes(ServerEnvironment *env,
			std::map<v3s16, MapBlock*>& modified_blocks,
			std::vector<std::pair<v3s16, MapNode>>& changed_nodes, IGameDef *gamedef)
	{
		// Find out whether there is a suspect for this action
		auto rb = gamedef->rollback();
		std::string suspect;
		if(rb != nullptr) {
			std::string suspect = rb->getSuspect(p[0], 83, 1);
		}

		for(u16 i = ALL_START; i < ALL_END; ++i) {

			if(n[i] == n_old[i]) continue;

			if(d[i]->isLiquid() && d_old[i]->floodable &&
					n_old[i].getContent() != CONTENT_AIR) {
				if (env->getScriptIface()->node_on_flood(p[i], n_old[i], n[i]))
					continue;
			}

			if(!suspect.empty()) {
				// Blame suspect
				RollbackScopeActor rollback_scope(rb, suspect, true);
				// Get old node for rollback
				RollbackNode rollback_oldnode(m_map, p[i], gamedef);
				// Set node
				m_map->setNode(p[i], n[i]);
				// Report
				RollbackNode rollback_newnode(m_map, p[i], gamedef);
				RollbackAction action;
				action.setSetNode(p[i], rollback_oldnode, rollback_newnode);
				rb->reportAction(action);
			}
			else {
				m_map->setNode(p[i], n[i]);
			}

			changed_nodes.emplace_back(p[i], n[i]);
			auto blockpos = getNodeBlockPos(p[i]);
			auto block = m_map->getBlockNoCreateNoEx(blockpos);
			if(block != nullptr) {
				modified_blocks[blockpos] = block;
			}

			ContentLightingFlags f = m_nodedef->getLightingFlags(n[i]);
			n[i].setLight(LIGHTBANK_DAY, 0, f);
			n[i].setLight(LIGHTBANK_NIGHT, 0, f);

		}
	}


	private:
	enum Dir
	{
		ALL_START = 0,
		C = 0, // Center
		OTHERS_START = 1,
		U = 1, // Up
		SAME_START = 2,
		B = 2, // Back
		R = 3, // Right
		F = 4, // Front
		L = 5, // Left
		SAME_END = 6,
		D = 6, // Down
		OTHERS_END = 7,
		ALL_END = 7,
		CNT_DIRS = 7,
	};

	bool isLiquid(const ContentFeatures *d)
	{
		return d->isLiquid() &&
			// This is a workaround for MCL.
			// MCL is abusing liquid for cobwebs.
			d->liquid_alternative_source_id != d->liquid_alternative_flowing_id;
	}

	bool isLiquid(Dir i) { return isLiquid(d[i]); }

	bool isLiquid(int i) { return isLiquid(d[i]); }

	bool isSameLiquid(int i, int j) {
		return isLiquid(i) && isLiquid(j) &&
			d[i]->liquid_alternative_source_id == d[j]->liquid_alternative_source_id
			;
	}

	bool levelInc(Dir i, int maxLevel)
	{
		int level = n[i].getLevel(m_nodedef);
		if(level >= maxLevel) return false;

		int increase = LIQUID_LEVEL_MAX - (int)d[i]->liquid_viscosity + 1;
		level += increase;
		if(level > maxLevel) level = maxLevel;
		if(level <= 0) return false;

		n[i].setLevel(m_nodedef, level);
		return true;
	}

	bool levelInit(int i, int maxLevel)
	{
		int level = LIQUID_LEVEL_MAX - (int)d[i]->liquid_viscosity + 1;
		//level = 1;
		if(level > maxLevel) level = maxLevel;
		if(level <= 0) return false;

		n[i].setLevel(m_nodedef, level);
		return true;
	}

	u8 getSlopeDistance(u8 liquid_level,
			const v3s16& dir)
	{
		v3s16 pi = p[0];
		for(u8 i = 0; i < liquid_level; ++i) {
			pi += dir;
			auto n1 = m_map->getNode(pi);
			auto& d1 = m_map->getNodeDefManager()->get(n1);
			if(d1.floodable || isLiquid(&d1)) {
				auto n2 = m_map->getNode(pi + v3s16(0, -1, 0));
				auto& d2 = m_map->getNodeDefManager()->get(n2);
				if(d2.floodable || isLiquid(&d2)) {
					return i;
				}
			}
			else {
				return UINT8_MAX;
			}
		}
		return UINT8_MAX;
	}

	bool handleRenewableLiquid(UniqueQueue<v3s16>& transforming_liquid)
	{
		if(d[C]->floodable ||
				(isLiquid(C) && d[C]->liquid_type == LIQUID_FLOWING)) {

			u8 cnt[CNT_DIRS] = {0};

			for(u16 i = SAME_START; i < SAME_END; ++i) {
				// Check if the liquid fits the requirements.
				if(d[i]->liquid_type != LIQUID_SOURCE || !d[i]->liquid_renewable ||
						!isLiquid(i)) {
					continue;
				}

				// Count how many times this liquid type appears.
				for(u16 j = SAME_START; j < SAME_END; ++j) {
					cnt[i] += (n[i].getContent() == n[j].getContent());
				}

				// If the number of sources of the same type fits the minal requirement
				// the center node turns into that type of liquid.
				if(cnt[i] >= 2 &&
						(d[C]->floodable ||
						 (d[C]->liquid_alternative_source_id == n[i].getContent()))
						) {

					transforming_liquid.push_back(p[C]);
					n[C] = MapNode(n[i]);
					d[C] = &m_nodedef->get(n[C]);
					return true;
				}
			}
		}
		return false;
	}

	bool handleSinkingLiquid(UniqueQueue<v3s16>& transforming_liquid)
	{
		if(n[C] == n_old[C] && isLiquid(C) && !isLiquid(U) &&
				d[C]->liquid_type == LIQUID_FLOWING) {
			// There is no liquid of the same type on top.
			u8 maxLevel = 0;

			u8 levels[CNT_DIRS] = {0};
			// find the biggest surrounding level.
			for(u16 i = SAME_START; i < SAME_END; ++i) {
				if(d[i]->liquid_alternative_flowing_id == n[C].getContent()) {
					levels[i] = n[i].getLevel(m_nodedef);
					if(levels[i] > maxLevel) {
						maxLevel = levels[i];
					}
				}
			}

			levels[C] = n[C].getLevel(m_nodedef);

			if(levels[C] >= maxLevel) {
				// The liquid shall sink.
				u8 new_l = maxLevel > 0? maxLevel-1 : 0;

				n[C].setLevel(m_nodedef, new_l);
				d[C] = &m_nodedef->get(n[C]);

				for(u16 i = SAME_START; i < SAME_END; ++i) {
					if(levels[i] >= maxLevel) transforming_liquid.push_back(p[i]);
				}

				if(new_l == 0) {
					transforming_liquid.push_back(p[C]);
				}
				return true;
			}
		}
		return false;
	}

	bool handleRemovedLiquid(UniqueQueue<v3s16>& transforming_liquid)
	{
		if(!isLiquid(C)) {
			for(u16 i = SAME_START; i < ALL_END; ++i) {
				if(isLiquid(i) && d[i]->liquid_type == LIQUID_FLOWING) {
					transforming_liquid.push_back(p[i]);
				}
			}
			return true;
		}
		return false;
	}

	bool handleFlowDownLiquid(UniqueQueue<v3s16>& transforming_liquid)
	{
		if(n[C] == n_old[C] && isLiquid(C) &&
				d[D]->floodable) {

			transforming_liquid.push_back(p[D]);
			n[D] = MapNode(n[C]);
			d[D] = &m_nodedef->get(n[D]); // levelInit() requires d[D]!!
			levelInit(D, LIQUID_LEVEL_SOURCE - 1);
			d[D] = &m_nodedef->get(n[D]);
			return true;
		}
		return false;
	}

	bool handleViscosityLiquid(UniqueQueue<v3s16>& transforming_liquid)
	{
		int levelC = n[C].getLevel(m_nodedef);
		if(isSameLiquid(C, U)) {
			if(levelC < LIQUID_LEVEL_MAX) {
				if(levelInc(C, LIQUID_LEVEL_MAX)) {
					d[C] = &m_nodedef->get(n[C]);
					transforming_liquid.push_back(p[C]);
					return true;
				}
			}
		}
		else if(isLiquid(C)) {
			int maxLevel = 0;
			for(u16 i = SAME_START; i < SAME_END; ++i) {
				if(!isSameLiquid(C, i)) continue;

				int level = n[i].getLevel(m_nodedef);
				if(level > maxLevel) maxLevel = level;
			}
			if(maxLevel - 1 > levelC) {
				if(levelInc(C, maxLevel - 1)) {
					d[C] = &m_nodedef->get(n[C]);
					transforming_liquid.push_back(p[C]);
					return true;
				}
			}
		}
		return false;
	}

	bool handleSpreadingLiquid(UniqueQueue<v3s16>& transforming_liquid)
	{
		if(n[C] == n_old[C] && isLiquid(C) &&
				!d[D]->floodable && !isLiquid(D)) {

			u8 l0 = n[C].getLevel(m_nodedef);
			if(l0 <= 1 || l0 <= (LIQUID_LEVEL_SOURCE - d[C]->liquid_range)) {
				// The liquid cannot spread further.
				return false;
			}

			if(d[C]->liquid_slope_range > 0) {

				int l = l0 + d[C]->liquid_slope_range - LIQUID_LEVEL_SOURCE;
				u8 max_slope_dist = (u8)(l < 0? 0 : l);

				u8 slope_dist[CNT_DIRS];

				// Calculate all the slope distances
				for(u16 i = SAME_START; i < SAME_END; ++i) {
					slope_dist[i] = getSlopeDistance(max_slope_dist, liquid_7dirs[i]);
				}

				slope_dist[C] = slope_dist[U] = slope_dist[D] = UINT8_MAX;

				// Find nearest slope.
				u8 min_slope_dist = UINT8_MAX;
				for(u16 i = SAME_START; i < SAME_END; ++i) {
					if(slope_dist[i] < min_slope_dist) {
						min_slope_dist = slope_dist[i];
					}
				}

				// Put liquid in the direction where the slope distance is the
				// shortest.
				for(u16 i = SAME_START; i < SAME_END; ++i) {
					if(d[i]->floodable && slope_dist[i] == min_slope_dist) {

						n[i] = MapNode(n[C]);
						d[i] = &m_nodedef->get(n[i]); // levelInit() requires d[i]!!
						levelInit(i, (int)l0 - 1);
						d[i] = &m_nodedef->get(n[i]);
						transforming_liquid.push_back(p[i]);
					}
				}
				return true;
			}
			else if(d[C]->liquid_slope_range == 0) {
				for(u16 i = SAME_START; i < SAME_END; ++i) {
					if(d[i]->floodable) {
						n[i] = MapNode(n[C]);
						d[i] = &m_nodedef->get(n[i]); // levelInit() requires d[i]!!
						levelInit(i, (int)l0 - 1);
						d[i] = &m_nodedef->get(n[i]);
						transforming_liquid.push_back(p[i]);
					}
				}
				return true;
			}
		}
		return false;
	}


	ServerMap *m_map;
	const NodeDefManager *m_nodedef;

	v3s16 p[CNT_DIRS];
	MapNode n[CNT_DIRS];
	MapNode n_old[CNT_DIRS];
	const ContentFeatures *d[CNT_DIRS];
	const ContentFeatures *d_old[CNT_DIRS];

};


void ServerMap::transformLiquids(std::map<v3s16, MapBlock*> &modified_blocks,
		ServerEnvironment *env)
{
	std::vector<std::pair<v3s16, MapNode>> changed_nodes;

	int cnt_nodes = m_transforming_liquid.size();
	for (int i = 0; i < cnt_nodes && m_transforming_liquid.size() > 0; ++i) {

		LiquidSystem liquidSystem(this);
		auto p0 = m_transforming_liquid.front();
		m_transforming_liquid.pop_front();

		liquidSystem.enterNode(p0, m_transforming_liquid);
		liquidSystem.writeChangedNodes(env, modified_blocks, changed_nodes,
				m_gamedef);
	}
	env->getScriptIface()->on_liquid_transformed(changed_nodes);
	voxalgo::update_lighting_nodes(this, changed_nodes, modified_blocks);
}


std::vector<v3s16> Map::findNodesWithMetadata(v3s16 p1, v3s16 p2)
{
	std::vector<v3s16> positions_with_meta;

	sortBoxVerticies(p1, p2);
	v3s16 bpmin = getNodeBlockPos(p1);
	v3s16 bpmax = getNodeBlockPos(p2);

	VoxelArea area(p1, p2);

	for (s16 z = bpmin.Z; z <= bpmax.Z; z++)
	for (s16 y = bpmin.Y; y <= bpmax.Y; y++)
	for (s16 x = bpmin.X; x <= bpmax.X; x++) {
		v3s16 blockpos(x, y, z);

		MapBlock *block = getBlockNoCreateNoEx(blockpos);
		if (!block) {
			verbosestream << "Map::getNodeMetadata(): Need to emerge "
				<< blockpos << std::endl;
			block = emergeBlock(blockpos, false);
		}
		if (!block) {
			infostream << "WARNING: Map::getNodeMetadata(): Block not found"
				<< std::endl;
			continue;
		}

		v3s16 p_base = blockpos * MAP_BLOCKSIZE;
		std::vector<v3s16> keys = block->m_node_metadata.getAllKeys();
		for (size_t i = 0; i != keys.size(); i++) {
			v3s16 p(keys[i] + p_base);
			if (!area.contains(p))
				continue;

			positions_with_meta.push_back(p);
		}
	}

	return positions_with_meta;
}

NodeMetadata *Map::getNodeMetadata(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::getNodeMetadata(): Need to emerge "
				<<blockpos<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::getNodeMetadata(): Block not found"
				<<std::endl;
		return NULL;
	}
	NodeMetadata *meta = block->m_node_metadata.get(p_rel);
	return meta;
}

bool Map::setNodeMetadata(v3s16 p, NodeMetadata *meta)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::setNodeMetadata(): Need to emerge "
				<<blockpos<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::setNodeMetadata(): Block not found"
				<<std::endl;
		return false;
	}
	block->m_node_metadata.set(p_rel, meta);
	return true;
}

void Map::removeNodeMetadata(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
	{
		warningstream<<"Map::removeNodeMetadata(): Block not found"
				<<std::endl;
		return;
	}
	block->m_node_metadata.remove(p_rel);
}

NodeTimer Map::getNodeTimer(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::getNodeTimer(): Need to emerge "
				<<blockpos<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::getNodeTimer(): Block not found"
				<<std::endl;
		return NodeTimer();
	}
	NodeTimer t = block->getNodeTimer(p_rel);
	NodeTimer nt(t.timeout, t.elapsed, p);
	return nt;
}

void Map::setNodeTimer(const NodeTimer &t)
{
	v3s16 p = t.position;
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(!block){
		infostream<<"Map::setNodeTimer(): Need to emerge "
				<<blockpos<<std::endl;
		block = emergeBlock(blockpos, false);
	}
	if(!block){
		warningstream<<"Map::setNodeTimer(): Block not found"
				<<std::endl;
		return;
	}
	NodeTimer nt(t.timeout, t.elapsed, p_rel);
	block->setNodeTimer(nt);
}

void Map::removeNodeTimer(v3s16 p)
{
	v3s16 blockpos = getNodeBlockPos(p);
	v3s16 p_rel = p - blockpos*MAP_BLOCKSIZE;
	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if(block == NULL)
	{
		warningstream<<"Map::removeNodeTimer(): Block not found"
				<<std::endl;
		return;
	}
	block->removeNodeTimer(p_rel);
}

bool Map::determineAdditionalOcclusionCheck(const v3s16 &pos_camera,
	const core::aabbox3d<s16> &block_bounds, v3s16 &check)
{
	/*
		This functions determines the node inside the target block that is
		closest to the camera position. This increases the occlusion culling
		accuracy in straight and diagonal corridors.
		The returned position will be occlusion checked first in addition to the
		others (8 corners + center).
		No position is returned if
		- the closest node is a corner, corners are checked anyway.
		- the camera is inside the target block, it will never be occluded.
	*/
#define CLOSEST_EDGE(pos, bounds, axis) \
	((pos).axis <= (bounds).MinEdge.axis) ? (bounds).MinEdge.axis : \
	(bounds).MaxEdge.axis

	bool x_inside = (block_bounds.MinEdge.X <= pos_camera.X) &&
			(pos_camera.X <= block_bounds.MaxEdge.X);
	bool y_inside = (block_bounds.MinEdge.Y <= pos_camera.Y) &&
			(pos_camera.Y <= block_bounds.MaxEdge.Y);
	bool z_inside = (block_bounds.MinEdge.Z <= pos_camera.Z) &&
			(pos_camera.Z <= block_bounds.MaxEdge.Z);

	if (x_inside && y_inside && z_inside)
		return false; // Camera inside target mapblock

	// straight
	if (x_inside && y_inside) {
		check = v3s16(pos_camera.X, pos_camera.Y, 0);
		check.Z = CLOSEST_EDGE(pos_camera, block_bounds, Z);
		return true;
	} else if (y_inside && z_inside) {
		check = v3s16(0, pos_camera.Y, pos_camera.Z);
		check.X = CLOSEST_EDGE(pos_camera, block_bounds, X);
		return true;
	} else if (x_inside && z_inside) {
		check = v3s16(pos_camera.X, 0, pos_camera.Z);
		check.Y = CLOSEST_EDGE(pos_camera, block_bounds, Y);
		return true;
	}

	// diagonal
	if (x_inside) {
		check = v3s16(pos_camera.X, 0, 0);
		check.Y = CLOSEST_EDGE(pos_camera, block_bounds, Y);
		check.Z = CLOSEST_EDGE(pos_camera, block_bounds, Z);
		return true;
	} else if (y_inside) {
		check = v3s16(0, pos_camera.Y, 0);
		check.X = CLOSEST_EDGE(pos_camera, block_bounds, X);
		check.Z = CLOSEST_EDGE(pos_camera, block_bounds, Z);
		return true;
	} else if (z_inside) {
		check = v3s16(0, 0, pos_camera.Z);
		check.X = CLOSEST_EDGE(pos_camera, block_bounds, X);
		check.Y = CLOSEST_EDGE(pos_camera, block_bounds, Y);
		return true;
	}

	// Closest node would be a corner, none returned
	return false;
}

bool Map::isOccluded(const v3s16 &pos_camera, const v3s16 &pos_target,
	float step, float stepfac, float offset, float end_offset, u32 needed_count)
{
	v3f direction = intToFloat(pos_target - pos_camera, BS);
	float distance = direction.getLength();

	// Normalize direction vector
	if (distance > 0.0f)
		direction /= distance;

	v3f pos_origin_f = intToFloat(pos_camera, BS);
	u32 count = 0;
	bool is_valid_position;

	for (; offset < distance + end_offset; offset += step) {
		v3f pos_node_f = pos_origin_f + direction * offset;
		v3s16 pos_node = floatToInt(pos_node_f, BS);

		MapNode node = getNode(pos_node, &is_valid_position);

		if (is_valid_position &&
				!m_nodedef->getLightingFlags(node).light_propagates) {
			// Cannot see through light-blocking nodes --> occluded
			count++;
			if (count >= needed_count)
				return true;
		}
		step *= stepfac;
	}
	return false;
}

bool Map::isBlockOccluded(MapBlock *block, v3s16 cam_pos_nodes)
{
	// Check occlusion for center and all 8 corners of the mapblock
	// Overshoot a little for less flickering
	static const s16 bs2 = MAP_BLOCKSIZE / 2 + 1;
	static const v3s16 dir9[9] = {
		v3s16( 0,  0,  0),
		v3s16( 1,  1,  1) * bs2,
		v3s16( 1,  1, -1) * bs2,
		v3s16( 1, -1,  1) * bs2,
		v3s16( 1, -1, -1) * bs2,
		v3s16(-1,  1,  1) * bs2,
		v3s16(-1,  1, -1) * bs2,
		v3s16(-1, -1,  1) * bs2,
		v3s16(-1, -1, -1) * bs2,
	};

	v3s16 pos_blockcenter = block->getPosRelative() + (MAP_BLOCKSIZE / 2);

	// Starting step size, value between 1m and sqrt(3)m
	float step = BS * 1.2f;
	// Multiply step by each iteraction by 'stepfac' to reduce checks in distance
	float stepfac = 1.05f;

	float start_offset = BS * 1.0f;

	// The occlusion search of 'isOccluded()' must stop short of the target
	// point by distance 'end_offset' to not enter the target mapblock.
	// For the 8 mapblock corners 'end_offset' must therefore be the maximum
	// diagonal of a mapblock, because we must consider all view angles.
	// sqrt(1^2 + 1^2 + 1^2) = 1.732
	float end_offset = -BS * MAP_BLOCKSIZE * 1.732f;

	// to reduce the likelihood of falsely occluded blocks
	// require at least two solid blocks
	// this is a HACK, we should think of a more precise algorithm
	u32 needed_count = 2;

	// Additional occlusion check, see comments in that function
	v3s16 check;
	if (determineAdditionalOcclusionCheck(cam_pos_nodes, block->getBox(), check)) {
		// node is always on a side facing the camera, end_offset can be lower
		if (!isOccluded(cam_pos_nodes, check, step, stepfac, start_offset,
				-1.0f, needed_count))
			return false;
	}

	for (const v3s16 &dir : dir9) {
		if (!isOccluded(cam_pos_nodes, pos_blockcenter + dir, step, stepfac,
				start_offset, end_offset, needed_count))
			return false;
	}
	return true;
}

/*
	ServerMap
*/
ServerMap::ServerMap(const std::string &savedir, IGameDef *gamedef,
		EmergeManager *emerge, MetricsBackend *mb):
	Map(gamedef),
	settings_mgr(savedir + DIR_DELIM + "map_meta.txt"),
	m_emerge(emerge)
{
	verbosestream<<FUNCTION_NAME<<std::endl;

	// Tell the EmergeManager about our MapSettingsManager
	emerge->map_settings_mgr = &settings_mgr;

	/*
		Try to load map; if not found, create a new one.
	*/

	// Determine which database backend to use
	std::string conf_path = savedir + DIR_DELIM + "world.mt";
	Settings conf;
	bool succeeded = conf.readConfigFile(conf_path.c_str());
	if (!succeeded || !conf.exists("backend")) {
		// fall back to sqlite3
		conf.set("backend", "sqlite3");
	}
	std::string backend = conf.get("backend");
	dbase = createDatabase(backend, savedir, conf);
	if (conf.exists("readonly_backend")) {
		std::string readonly_dir = savedir + DIR_DELIM + "readonly";
		dbase_ro = createDatabase(conf.get("readonly_backend"), readonly_dir, conf);
	}
	if (!conf.updateConfigFile(conf_path.c_str()))
		errorstream << "ServerMap::ServerMap(): Failed to update world.mt!" << std::endl;

	m_savedir = savedir;
	m_map_saving_enabled = false;

	m_save_time_counter = mb->addCounter(
		"minetest_map_save_time", "Time spent saving blocks (in microseconds)");
	m_save_count_counter = mb->addCounter(
		"minetest_map_saved_blocks", "Number of blocks saved");
	m_loaded_blocks_gauge = mb->addGauge(
		"minetest_map_loaded_blocks", "Number of loaded blocks");

	m_map_compression_level = rangelim(g_settings->getS16("map_compression_level_disk"), -1, 9);

	try {
		// If directory exists, check contents and load if possible
		if (fs::PathExists(m_savedir)) {
			// If directory is empty, it is safe to save into it.
			if (fs::GetDirListing(m_savedir).empty()) {
				infostream<<"ServerMap: Empty save directory is valid."
						<<std::endl;
				m_map_saving_enabled = true;
			}
			else
			{

				if (settings_mgr.loadMapMeta()) {
					infostream << "ServerMap: Metadata loaded from "
						<< savedir << std::endl;
				} else {
					infostream << "ServerMap: Metadata could not be loaded "
						"from " << savedir << ", assuming valid save "
						"directory." << std::endl;
				}

				m_map_saving_enabled = true;
				// Map loaded, not creating new one
				return;
			}
		}
		// If directory doesn't exist, it is safe to save to it
		else{
			m_map_saving_enabled = true;
		}
	}
	catch(std::exception &e)
	{
		warningstream<<"ServerMap: Failed to load map from "<<savedir
				<<", exception: "<<e.what()<<std::endl;
		infostream<<"Please remove the map or fix it."<<std::endl;
		warningstream<<"Map saving will be disabled."<<std::endl;
	}
}

ServerMap::~ServerMap()
{
	verbosestream<<FUNCTION_NAME<<std::endl;

	try
	{
		if (m_map_saving_enabled) {
			// Save only changed parts
			save(MOD_STATE_WRITE_AT_UNLOAD);
			infostream << "ServerMap: Saved map to " << m_savedir << std::endl;
		} else {
			infostream << "ServerMap: Map not saved" << std::endl;
		}
	}
	catch(std::exception &e)
	{
		infostream<<"ServerMap: Failed to save map to "<<m_savedir
				<<", exception: "<<e.what()<<std::endl;
	}

	/*
		Close database if it was opened
	*/
	delete dbase;
	delete dbase_ro;

	deleteDetachedBlocks();
}

MapgenParams *ServerMap::getMapgenParams()
{
	// getMapgenParams() should only ever be called after Server is initialized
	assert(settings_mgr.mapgen_params != NULL);
	return settings_mgr.mapgen_params;
}

u64 ServerMap::getSeed()
{
	return getMapgenParams()->seed;
}

bool ServerMap::blockpos_over_mapgen_limit(v3s16 p)
{
	const s16 mapgen_limit_bp = rangelim(
		getMapgenParams()->mapgen_limit, 0, MAX_MAP_GENERATION_LIMIT) /
		MAP_BLOCKSIZE;
	return p.X < -mapgen_limit_bp ||
		p.X >  mapgen_limit_bp ||
		p.Y < -mapgen_limit_bp ||
		p.Y >  mapgen_limit_bp ||
		p.Z < -mapgen_limit_bp ||
		p.Z >  mapgen_limit_bp;
}

bool ServerMap::initBlockMake(v3s16 blockpos, BlockMakeData *data)
{
	s16 csize = getMapgenParams()->chunksize;
	v3s16 bpmin = EmergeManager::getContainingChunk(blockpos, csize);
	v3s16 bpmax = bpmin + v3s16(1, 1, 1) * (csize - 1);

	if (!m_chunks_in_progress.insert(bpmin).second)
		return false;

	bool enable_mapgen_debug_info = m_emerge->enable_mapgen_debug_info;
	EMERGE_DBG_OUT("initBlockMake(): " << bpmin << " - " << bpmax);

	v3s16 extra_borders(1, 1, 1);
	v3s16 full_bpmin = bpmin - extra_borders;
	v3s16 full_bpmax = bpmax + extra_borders;

	// Do nothing if not inside mapgen limits (+-1 because of neighbors)
	if (blockpos_over_mapgen_limit(full_bpmin) ||
			blockpos_over_mapgen_limit(full_bpmax))
		return false;

	data->seed = getSeed();
	data->blockpos_min = bpmin;
	data->blockpos_max = bpmax;
	data->nodedef = m_nodedef;

	/*
		Create the whole area of this and the neighboring blocks
	*/
	for (s16 x = full_bpmin.X; x <= full_bpmax.X; x++)
	for (s16 z = full_bpmin.Z; z <= full_bpmax.Z; z++) {
		v2s16 sectorpos(x, z);
		// Sector metadata is loaded from disk if not already loaded.
		MapSector *sector = createSector(sectorpos);
		FATAL_ERROR_IF(sector == NULL, "createSector() failed");

		for (s16 y = full_bpmin.Y; y <= full_bpmax.Y; y++) {
			v3s16 p(x, y, z);

			MapBlock *block = emergeBlock(p, false);
			if (block == NULL) {
				block = createBlock(p);

				// Block gets sunlight if this is true.
				// Refer to the map generator heuristics.
				bool ug = m_emerge->isBlockUnderground(p);
				block->setIsUnderground(ug);
			}
		}
	}

	/*
		Now we have a big empty area.

		Make a ManualMapVoxelManipulator that contains this and the
		neighboring blocks
	*/

	data->vmanip = new MMVManip(this);
	data->vmanip->initialEmerge(full_bpmin, full_bpmax);

	// Data is ready now.
	return true;
}

void ServerMap::finishBlockMake(BlockMakeData *data,
	std::map<v3s16, MapBlock*> *changed_blocks)
{
	v3s16 bpmin = data->blockpos_min;
	v3s16 bpmax = data->blockpos_max;

	bool enable_mapgen_debug_info = m_emerge->enable_mapgen_debug_info;
	EMERGE_DBG_OUT("finishBlockMake(): " << bpmin << " - " << bpmax);

	/*
		Blit generated stuff to map
		NOTE: blitBackAll adds nearly everything to changed_blocks
	*/
	data->vmanip->blitBackAll(changed_blocks);

	EMERGE_DBG_OUT("finishBlockMake: changed_blocks.size()="
		<< changed_blocks->size());

	/*
		Copy transforming liquid information
	*/
	while (data->transforming_liquid.size()) {
		m_transforming_liquid.push_back(data->transforming_liquid.front());
		data->transforming_liquid.pop_front();
	}

	for (auto &changed_block : *changed_blocks) {
		MapBlock *block = changed_block.second;
		if (!block)
			continue;
		/*
			Update day/night difference cache of the MapBlocks
		*/
		block->expireDayNightDiff();
		/*
			Set block as modified
		*/
		block->raiseModified(MOD_STATE_WRITE_NEEDED,
			MOD_REASON_EXPIRE_DAYNIGHTDIFF);
	}

	/*
		Set central blocks as generated
	*/
	for (s16 x = bpmin.X; x <= bpmax.X; x++)
	for (s16 z = bpmin.Z; z <= bpmax.Z; z++)
	for (s16 y = bpmin.Y; y <= bpmax.Y; y++) {
		MapBlock *block = getBlockNoCreateNoEx(v3s16(x, y, z));
		if (!block)
			continue;

		block->setGenerated(true);
	}

	/*
		Save changed parts of map
		NOTE: Will be saved later.
	*/
	//save(MOD_STATE_WRITE_AT_UNLOAD);
	m_chunks_in_progress.erase(bpmin);
}

MapSector *ServerMap::createSector(v2s16 p2d)
{
	/*
		Check if it exists already in memory
	*/
	MapSector *sector = getSectorNoGenerate(p2d);
	if (sector)
		return sector;

	/*
		Do not create over max mapgen limit
	*/
	if (blockpos_over_max_limit(v3s16(p2d.X, 0, p2d.Y)))
		throw InvalidPositionException("createSector(): pos. over max mapgen limit");

	/*
		Generate blank sector
	*/

	sector = new MapSector(this, p2d, m_gamedef);

	/*
		Insert to container
	*/
	m_sectors[p2d] = sector;

	return sector;
}

MapBlock * ServerMap::createBlock(v3s16 p)
{
	/*
		Do not create over max mapgen limit
	*/
	if (blockpos_over_max_limit(p))
		throw InvalidPositionException("createBlock(): pos. over max mapgen limit");

	v2s16 p2d(p.X, p.Z);
	s16 block_y = p.Y;
	/*
		This will create or load a sector if not found in memory.
		If block exists on disk, it will be loaded.

		NOTE: On old save formats, this will be slow, as it generates
		      lighting on blocks for them.
	*/
	MapSector *sector;
	try {
		sector = createSector(p2d);
	} catch (InvalidPositionException &e) {
		infostream<<"createBlock: createSector() failed"<<std::endl;
		throw e;
	}

	/*
		Try to get a block from the sector
	*/

	MapBlock *block = sector->getBlockNoCreateNoEx(block_y);
	if (block) {
		return block;
	}
	// Create blank
	block = sector->createBlankBlock(block_y);

	return block;
}

MapBlock * ServerMap::emergeBlock(v3s16 p, bool create_blank)
{
	{
		MapBlock *block = getBlockNoCreateNoEx(p);
		if (block)
			return block;
	}

	{
		MapBlock *block = loadBlock(p);
		if(block)
			return block;
	}

	if (create_blank) {
		MapSector *sector = createSector(v2s16(p.X, p.Z));
		MapBlock *block = sector->createBlankBlock(p.Y);

		return block;
	}

	return NULL;
}

MapBlock *ServerMap::getBlockOrEmerge(v3s16 p3d)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d);
	if (block == NULL)
		m_emerge->enqueueBlockEmerge(PEER_ID_INEXISTENT, p3d, false);

	return block;
}

bool ServerMap::isBlockInQueue(v3s16 pos)
{
	return m_emerge && m_emerge->isBlockInQueue(pos);
}

void ServerMap::addNodeAndUpdate(v3s16 p, MapNode n,
		std::map<v3s16, MapBlock*> &modified_blocks,
		bool remove_metadata)
{
	Map::addNodeAndUpdate(p, n, modified_blocks, remove_metadata);

	/*
		Add neighboring liquid nodes and this node to transform queue.
		(it's vital for the node itself to get updated last, if it was removed.)
	 */

	for (const v3s16 &dir : g_7dirs) {
		v3s16 p2 = p + dir;

		bool is_valid_position;
		MapNode n2 = getNode(p2, &is_valid_position);
		if(is_valid_position &&
				(m_nodedef->get(n2).isLiquid() ||
				n2.getContent() == CONTENT_AIR))
			m_transforming_liquid.push_back(p2);
	}
}

// N.B.  This requires no synchronization, since data will not be modified unless
// the VoxelManipulator being updated belongs to the same thread.
void ServerMap::updateVManip(v3s16 pos)
{
	Mapgen *mg = m_emerge->getCurrentMapgen();
	if (!mg)
		return;

	MMVManip *vm = mg->vm;
	if (!vm)
		return;

	if (!vm->m_area.contains(pos))
		return;

	s32 idx = vm->m_area.index(pos);
	vm->m_data[idx] = getNode(pos);
	vm->m_flags[idx] &= ~VOXELFLAG_NO_DATA;

	vm->m_is_dirty = true;
}

void ServerMap::reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks)
{
	m_loaded_blocks_gauge->set(all_blocks);
	m_save_time_counter->increment(save_time_us);
	m_save_count_counter->increment(saved_blocks);
}

void ServerMap::save(ModifiedState save_level)
{
	if (!m_map_saving_enabled) {
		warningstream<<"Not saving map, saving disabled."<<std::endl;
		return;
	}

	const auto start_time = porting::getTimeUs();

	if(save_level == MOD_STATE_CLEAN)
		infostream<<"ServerMap: Saving whole map, this can take time."
				<<std::endl;

	if (m_map_metadata_changed || save_level == MOD_STATE_CLEAN) {
		if (settings_mgr.saveMapMeta())
			m_map_metadata_changed = false;
	}

	// Profile modified reasons
	Profiler modprofiler;

	u32 block_count = 0;
	u32 block_count_all = 0; // Number of blocks in memory

	// Don't do anything with sqlite unless something is really saved
	bool save_started = false;

	for (auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;

		MapBlockVect blocks;
		sector->getBlocks(blocks);

		for (MapBlock *block : blocks) {
			block_count_all++;

			if(block->getModified() >= (u32)save_level) {
				// Lazy beginSave()
				if(!save_started) {
					beginSave();
					save_started = true;
				}

				modprofiler.add(block->getModifiedReasonString(), 1);

				saveBlock(block);
				block_count++;
			}
		}
	}

	if(save_started)
		endSave();

	/*
		Only print if something happened or saved whole map
	*/
	if(save_level == MOD_STATE_CLEAN
			|| block_count != 0) {
		infostream << "ServerMap: Written: "
				<< block_count << " blocks"
				<< ", " << block_count_all << " blocks in memory."
				<< std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Blocks modified by: "<<std::endl;
		modprofiler.print(infostream);
	}

	const auto end_time = porting::getTimeUs();
	reportMetrics(end_time - start_time, block_count, block_count_all);
}

void ServerMap::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	dbase->listAllLoadableBlocks(dst);
	if (dbase_ro)
		dbase_ro->listAllLoadableBlocks(dst);
}

void ServerMap::listAllLoadedBlocks(std::vector<v3s16> &dst)
{
	for (auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;

		MapBlockVect blocks;
		sector->getBlocks(blocks);

		for (MapBlock *block : blocks) {
			v3s16 p = block->getPos();
			dst.push_back(p);
		}
	}
}

MapDatabase *ServerMap::createDatabase(
	const std::string &name,
	const std::string &savedir,
	Settings &conf)
{
	if (name == "sqlite3")
		return new MapDatabaseSQLite3(savedir);
	if (name == "dummy")
		return new Database_Dummy();
	#if USE_LEVELDB
	if (name == "leveldb")
		return new Database_LevelDB(savedir);
	#endif
	#if USE_REDIS
	if (name == "redis")
		return new Database_Redis(conf);
	#endif
	#if USE_POSTGRESQL
	if (name == "postgresql") {
		std::string connect_string;
		conf.getNoEx("pgsql_connection", connect_string);
		return new MapDatabasePostgreSQL(connect_string);
	}
	#endif

	throw BaseException(std::string("Database backend ") + name + " not supported.");
}

void ServerMap::beginSave()
{
	dbase->beginSave();
}

void ServerMap::endSave()
{
	dbase->endSave();
}

bool ServerMap::saveBlock(MapBlock *block)
{
	return saveBlock(block, dbase, m_map_compression_level);
}

bool ServerMap::saveBlock(MapBlock *block, MapDatabase *db, int compression_level)
{
	v3s16 p3d = block->getPos();

	// Format used for writing
	u8 version = SER_FMT_VER_HIGHEST_WRITE;

	/*
		[0] u8 serialization version
		[1] data
	*/
	std::ostringstream o(std::ios_base::binary);
	o.write((char*) &version, 1);
	block->serialize(o, version, true, compression_level);

	bool ret = db->saveBlock(p3d, o.str());
	if (ret) {
		// We just wrote it to the disk so clear modified flag
		block->resetModified();
	}
	return ret;
}

void ServerMap::loadBlock(std::string *blob, v3s16 p3d, MapSector *sector, bool save_after_load)
{
	try {
		std::istringstream is(*blob, std::ios_base::binary);

		u8 version = SER_FMT_VER_INVALID;
		is.read((char*)&version, 1);

		if(is.fail())
			throw SerializationError("ServerMap::loadBlock(): Failed"
					" to read MapBlock version");

		MapBlock *block = nullptr;
		std::unique_ptr<MapBlock> block_created_new;
		block = sector->getBlockNoCreateNoEx(p3d.Y);
		if (!block) {
			block_created_new = sector->createBlankBlockNoInsert(p3d.Y);
			block = block_created_new.get();
		}

		{
		ScopeProfiler sp(g_profiler, "ServerMap: deSer block", SPT_AVG);
		// Read basic data
		block->deSerialize(is, version, true);
		}

		// If it's a new block, insert it to the map
		if (block_created_new) {
			sector->insertBlock(std::move(block_created_new));
			ReflowScan scanner(this, m_emerge->ndef);
			scanner.scan(block, &m_transforming_liquid);
		}

		/*
			Save blocks loaded in old format in new format
		*/

		//if(version < SER_FMT_VER_HIGHEST_READ || save_after_load)
		// Only save if asked to; no need to update version
		if(save_after_load)
			saveBlock(block);

		// We just loaded it from, so it's up-to-date.
		block->resetModified();
	}
	catch(SerializationError &e)
	{
		errorstream<<"Invalid block data in database"
				<<" ("<<p3d.X<<","<<p3d.Y<<","<<p3d.Z<<")"
				<<" (SerializationError): "<<e.what()<<std::endl;

		// TODO: Block should be marked as invalid in memory so that it is
		// not touched but the game can run

		if(g_settings->getBool("ignore_world_load_errors")){
			errorstream<<"Ignoring block load error. Duck and cover! "
					<<"(ignore_world_load_errors)"<<std::endl;
		} else {
			throw SerializationError("Invalid block data in database");
		}
	}
}

MapBlock* ServerMap::loadBlock(v3s16 blockpos)
{
	ScopeProfiler sp(g_profiler, "ServerMap: load block", SPT_AVG);
	bool created_new = (getBlockNoCreateNoEx(blockpos) == NULL);

	v2s16 p2d(blockpos.X, blockpos.Z);

	std::string ret;
	dbase->loadBlock(blockpos, &ret);
	if (!ret.empty()) {
		loadBlock(&ret, blockpos, createSector(p2d), false);
	} else if (dbase_ro) {
		dbase_ro->loadBlock(blockpos, &ret);
		if (!ret.empty()) {
			loadBlock(&ret, blockpos, createSector(p2d), false);
		}
	} else {
		return NULL;
	}

	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (created_new && (block != NULL)) {
		std::map<v3s16, MapBlock*> modified_blocks;
		// Fix lighting if necessary
		voxalgo::update_block_border_lighting(this, block, modified_blocks);
		if (!modified_blocks.empty()) {
			//Modified lighting, send event
			MapEditEvent event;
			event.type = MEET_OTHER;
			event.setModifiedBlocks(modified_blocks);
			dispatchEvent(event);
		}
	}
	return block;
}

bool ServerMap::deleteBlock(v3s16 blockpos)
{
	if (!dbase->deleteBlock(blockpos))
		return false;

	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (block) {
		v2s16 p2d(blockpos.X, blockpos.Z);
		MapSector *sector = getSectorNoGenerate(p2d);
		if (!sector)
			return false;
		// It may not be safe to delete the block from memory at the moment
		// (pointers to it could still be in use)
		m_detached_blocks.push_back(sector->detachBlock(block));
	}

	return true;
}

void ServerMap::deleteDetachedBlocks()
{
	for (const auto &block : m_detached_blocks) {
		assert(block->isOrphan());
		(void)block; // silence unused-variable warning in release builds
	}

	m_detached_blocks.clear();
}

void ServerMap::step()
{
	// Delete from memory blocks removed by deleteBlocks() only when pointers
	// to them are (probably) no longer in use
	deleteDetachedBlocks();
}

void ServerMap::PrintInfo(std::ostream &out)
{
	out<<"ServerMap: ";
}

bool ServerMap::repairBlockLight(v3s16 blockpos,
	std::map<v3s16, MapBlock *> *modified_blocks)
{
	MapBlock *block = emergeBlock(blockpos, false);
	if (!block || !block->isGenerated())
		return false;
	voxalgo::repair_block_light(this, block, modified_blocks);
	return true;
}

MMVManip::MMVManip(Map *map):
		VoxelManipulator(),
		m_map(map)
{
	assert(map);
}

void MMVManip::initialEmerge(v3s16 blockpos_min, v3s16 blockpos_max,
	bool load_if_inexistent)
{
	TimeTaker timer1("initialEmerge", &emerge_time);

	assert(m_map);

	// Units of these are MapBlocks
	v3s16 p_min = blockpos_min;
	v3s16 p_max = blockpos_max;

	VoxelArea block_area_nodes
			(p_min*MAP_BLOCKSIZE, (p_max+1)*MAP_BLOCKSIZE-v3s16(1,1,1));

	u32 size_MB = block_area_nodes.getVolume()*4/1000000;
	if(size_MB >= 1)
	{
		infostream<<"initialEmerge: area: ";
		block_area_nodes.print(infostream);
		infostream<<" ("<<size_MB<<"MB)";
		infostream<<std::endl;
	}

	addArea(block_area_nodes);

	for(s32 z=p_min.Z; z<=p_max.Z; z++)
	for(s32 y=p_min.Y; y<=p_max.Y; y++)
	for(s32 x=p_min.X; x<=p_max.X; x++)
	{
		u8 flags = 0;
		MapBlock *block;
		v3s16 p(x,y,z);
		std::map<v3s16, u8>::iterator n;
		n = m_loaded_blocks.find(p);
		if(n != m_loaded_blocks.end())
			continue;

		bool block_data_inexistent = false;
		{
			TimeTaker timer2("emerge load", &emerge_load_time);

			block = m_map->getBlockNoCreateNoEx(p);
			if (!block)
				block_data_inexistent = true;
			else
				block->copyTo(*this);
		}

		if(block_data_inexistent)
		{

			if (load_if_inexistent && !blockpos_over_max_limit(p)) {
				ServerMap *svrmap = (ServerMap *)m_map;
				block = svrmap->emergeBlock(p, false);
				if (block == NULL)
					block = svrmap->createBlock(p);
				block->copyTo(*this);
			} else {
				flags |= VMANIP_BLOCK_DATA_INEXIST;

				/*
					Mark area inexistent
				*/
				VoxelArea a(p*MAP_BLOCKSIZE, (p+1)*MAP_BLOCKSIZE-v3s16(1,1,1));
				// Fill with VOXELFLAG_NO_DATA
				for(s32 z=a.MinEdge.Z; z<=a.MaxEdge.Z; z++)
				for(s32 y=a.MinEdge.Y; y<=a.MaxEdge.Y; y++)
				{
					s32 i = m_area.index(a.MinEdge.X,y,z);
					memset(&m_flags[i], VOXELFLAG_NO_DATA, MAP_BLOCKSIZE);
				}
			}
		}
		/*else if (block->getNode(0, 0, 0).getContent() == CONTENT_IGNORE)
		{
			// Mark that block was loaded as blank
			flags |= VMANIP_BLOCK_CONTAINS_CIGNORE;
		}*/

		m_loaded_blocks[p] = flags;
	}

	m_is_dirty = false;
}

void MMVManip::blitBackAll(std::map<v3s16, MapBlock*> *modified_blocks,
	bool overwrite_generated)
{
	if(m_area.getExtent() == v3s16(0,0,0))
		return;
	assert(m_map);

	/*
		Copy data of all blocks
	*/
	for (auto &loaded_block : m_loaded_blocks) {
		v3s16 p = loaded_block.first;
		MapBlock *block = m_map->getBlockNoCreateNoEx(p);
		bool existed = !(loaded_block.second & VMANIP_BLOCK_DATA_INEXIST);
		if (!existed || (block == NULL) ||
			(!overwrite_generated && block->isGenerated()))
			continue;

		block->copyFrom(*this);
		block->raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_VMANIP);

		if(modified_blocks)
			(*modified_blocks)[p] = block;
	}
}

MMVManip *MMVManip::clone() const
{
	MMVManip *ret = new MMVManip();

	const s32 size = m_area.getVolume();
	ret->m_area = m_area;
	if (m_data) {
		ret->m_data = new MapNode[size];
		memcpy(ret->m_data, m_data, size * sizeof(MapNode));
	}
	if (m_flags) {
		ret->m_flags = new u8[size];
		memcpy(ret->m_flags, m_flags, size * sizeof(u8));
	}

	ret->m_is_dirty = m_is_dirty;
	// Even if the copy is disconnected from a map object keep the information
	// needed to write it back to one
	ret->m_loaded_blocks = m_loaded_blocks;

	return ret;
}

void MMVManip::reparent(Map *map)
{
	assert(map && !m_map);
	m_map = map;
}

//END
