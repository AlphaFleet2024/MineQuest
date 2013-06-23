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

#ifndef MAPGEN_HEADER
#define MAPGEN_HEADER

#include "irrlichttypes_extrabloated.h"
#include "util/container.h" // UniqueQueue
#include "gamedef.h"
#include "mapnode.h"
#include "noise.h"
#include "settings.h"
#include <map>

/////////////////// Mapgen flags
#define MG_TREES         0x01
#define MG_CAVES         0x02
#define MG_DUNGEONS      0x04
#define MGV6_JUNGLES     0x08
#define MGV6_BIOME_BLEND 0x10
#define MG_FLAT          0x20

/////////////////// Ore generation flags
// Use absolute value of height to determine ore placement
#define OREFLAG_ABSHEIGHT 0x01 
// Use 3d noise to get density of ore placement, instead of just the position
#define OREFLAG_DENSITY   0x02 // not yet implemented
// For claylike ore types, place ore if the number of surrounding
// nodes isn't the specified node
#define OREFLAG_NODEISNT  0x04 // not yet implemented

/////////////////// Decoration flags
#define DECO_PLACE_CENTER_X 1
#define DECO_PLACE_CENTER_Y 2
#define DECO_PLACE_CENTER_Z 4

extern FlagDesc flagdesc_mapgen[];
extern FlagDesc flagdesc_ore[];
extern FlagDesc flagdesc_deco_schematic[];

class BiomeDefManager;
class Biome;
class EmergeManager;
class MapBlock;
class ManualMapVoxelManipulator;
class VoxelManipulator;
class INodeDefManager;
struct BlockMakeData;
class VoxelArea;
class Map;

struct MapgenParams {
	std::string mg_name;
	int chunksize;
	u64 seed;
	int water_level;
	u32 flags;

	MapgenParams() {
		mg_name     = "v6";
		seed        = 0;
		water_level = 1;
		chunksize   = 5;
		flags       = MG_TREES | MG_CAVES | MGV6_BIOME_BLEND;
	}
	
	virtual bool readParams(Settings *settings) = 0;
	virtual void writeParams(Settings *settings) = 0;
	virtual ~MapgenParams() {}
};

class Mapgen {
public:
	int seed;
	int water_level;
	bool generating;
	int id;
	ManualMapVoxelManipulator *vm;
	INodeDefManager *ndef;
	s16 *heightmap;
	u8 *biomemap;

	Mapgen();
	virtual ~Mapgen() {}

	s16 findGroundLevelFull(v2s16 p2d);
	s16 findGroundLevel(v2s16 p2d, s16 ymin, s16 ymax);
	void updateHeightmap(v3s16 nmin, v3s16 nmax);
	void updateLiquid(UniqueQueue<v3s16> *trans_liquid, v3s16 nmin, v3s16 nmax);
	void setLighting(v3s16 nmin, v3s16 nmax, u8 light);
	void lightSpread(VoxelArea &a, v3s16 p, u8 light);
	void calcLighting(v3s16 nmin, v3s16 nmax);
	void calcLightingOld(v3s16 nmin, v3s16 nmax);

	virtual void makeChunk(BlockMakeData *data) {};
	virtual int getGroundLevelAtPoint(v2s16 p) = 0;

	//Legacy functions for Farmesh (pending removal)
	static bool get_have_beach(u64 seed, v2s16 p2d);
	static double tree_amount_2d(u64 seed, v2s16 p);
	static s16 find_ground_level_from_noise(u64 seed, v2s16 p2d, s16 precision);
};

struct MapgenFactory {
	virtual Mapgen *createMapgen(int mgid, MapgenParams *params,
								 EmergeManager *emerge) = 0;
	virtual MapgenParams *createMapgenParams() = 0;
	virtual ~MapgenFactory() {}
};

enum OreType {
	ORE_SCATTER,
	ORE_SHEET,
	ORE_CLAYLIKE
};

#define ORE_RANGE_ACTUAL 1
#define ORE_RANGE_MIRROR 2

class Ore {
public:
	std::string ore_name;
	std::string wherein_name;
	content_t ore;
	content_t wherein;  // the node to be replaced
	u32 clust_scarcity; // ore cluster has a 1-in-clust_scarcity chance of appearing at a node
	s16 clust_num_ores; // how many ore nodes are in a chunk
	s16 clust_size;     // how large (in nodes) a chunk of ore is
	s16 height_min;
	s16 height_max;
	u8 ore_param2;		// to set node-specific attributes
	u32 flags;          // attributes for this ore
	float nthresh;      // threshhold for noise at which an ore is placed 
	NoiseParams *np;    // noise for distribution of clusters (NULL for uniform scattering)
	Noise *noise;
	
	Ore() {
		ore     = CONTENT_IGNORE;
		wherein = CONTENT_IGNORE;
		np      = NULL;
		noise   = NULL;
	}
	
	virtual ~Ore();
	
	void resolveNodeNames(INodeDefManager *ndef);
	void placeOre(Mapgen *mg, u32 blockseed, v3s16 nmin, v3s16 nmax);
	virtual void generate(ManualMapVoxelManipulator *vm, int seed,
						u32 blockseed, v3s16 nmin, v3s16 nmax) = 0;
};

class OreScatter : public Ore {
	~OreScatter() {}
	virtual void generate(ManualMapVoxelManipulator *vm, int seed,
						u32 blockseed, v3s16 nmin, v3s16 nmax);
};

class OreSheet : public Ore {
	~OreSheet() {}
	virtual void generate(ManualMapVoxelManipulator *vm, int seed,
						u32 blockseed, v3s16 nmin, v3s16 nmax);
};

Ore *createOre(OreType type);


enum DecorationType {
	DECO_SIMPLE,
	DECO_SCHEMATIC,
	DECO_LSYSTEM
};

#if 0
struct CutoffData {
	VoxelArea a;
	Decoration *deco;
	//v3s16 p;
	//v3s16 size;
	//s16 height;
	
	CutoffData(s16 x, s16 y, s16 z, s16 h) {
		p = v3s16(x, y, z);
		height = h;
	}
};
#endif

class Decoration {
public:
	int mapseed;
	std::string place_on_name;
	content_t c_place_on;
	s16 sidelen;
	float fill_ratio;
	NoiseParams *np;
	
	std::set<u8> biomes;
	//std::list<CutoffData> cutoffs;
	//JMutex cutoff_mutex;

	Decoration();
	virtual ~Decoration();
	
	virtual void resolveNodeNames(INodeDefManager *ndef);
	void placeDeco(Mapgen *mg, u32 blockseed, v3s16 nmin, v3s16 nmax);
	void placeCutoffs(Mapgen *mg, u32 blockseed, v3s16 nmin, v3s16 nmax);
	
	virtual void generate(Mapgen *mg, PseudoRandom *pr, s16 max_y, v3s16 p) = 0;
	virtual int getHeight() = 0;
	virtual std::string getName() = 0;
};

class DecoSimple : public Decoration {
public:
	std::string deco_name;
	std::string spawnby_name;
	content_t c_deco;
	content_t c_spawnby;
	s16 deco_height;
	s16 deco_height_max;
	s16 nspawnby;
	
	std::vector<std::string> decolist_names;
	std::vector<content_t> c_decolist;

	~DecoSimple() {}
	
	void resolveNodeNames(INodeDefManager *ndef);
	virtual void generate(Mapgen *mg, PseudoRandom *pr, s16 max_y, v3s16 p);
	virtual int getHeight();
	virtual std::string getName();
};

class DecoSchematic : public Decoration {
public:
	std::string filename;
	
	std::vector<std::string> *node_names;
	std::vector<content_t> c_nodes;

	u32 flags;
	v3s16 size;
	MapNode *schematic;

	DecoSchematic();
	~DecoSchematic();
	
	void resolveNodeNames(INodeDefManager *ndef);
	virtual void generate(Mapgen *mg, PseudoRandom *pr, s16 max_y, v3s16 p);
	virtual int getHeight();
	virtual std::string getName();
	
	bool loadSchematicFile();
	void saveSchematicFile(INodeDefManager *ndef);
	
	bool getSchematicFromMap(Map *map, v3s16 p1, v3s16 p2);
	void placeStructure(Map *map, v3s16 p);
	void applyProbabilities(std::vector<std::pair<v3s16, s16> > *plist, v3s16 p0);
};

void build_nnlist_and_update_ids(MapNode *nodes, u32 nodecount,
					std::vector<content_t> *usednodes);

/*
class DecoLSystem : public Decoration {
public:
	virtual void generate(Mapgen *mg, u32 blockseed, v3s16 nmin, v3s16 nmax);
};
*/

Decoration *createDecoration(DecorationType type);

#endif

