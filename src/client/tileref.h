#pragma once
#include <cassert>
#include "tile.h"

struct LayerRef
{
	const TileSpec *tile = nullptr;
	video::SColor color = video::SColor(0xFFFFFFFF);
	u8 material_flags = 0;
	u8 rotation = 0;
	u8 emissive_light = 0;
	u8 layer = 0;

	const TileLayer &get() const
	{
		return tile->layers[layer];
	}

	// Sets everything else except the texture in the material
	void applyMaterialOptions(video::SMaterial &material) const
	{
		switch (get().material_type) {
		case TILE_MATERIAL_OPAQUE:
		case TILE_MATERIAL_LIQUID_OPAQUE:
			material.MaterialType = video::EMT_SOLID;
			break;
		case TILE_MATERIAL_BASIC:
		case TILE_MATERIAL_WAVING_LEAVES:
		case TILE_MATERIAL_WAVING_PLANTS:
			material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			break;
		case TILE_MATERIAL_ALPHA:
		case TILE_MATERIAL_LIQUID_TRANSPARENT:
			material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			break;
		default:
			break;
		}
		material.BackfaceCulling = (material_flags & MATERIAL_FLAG_BACKFACE_CULLING) != 0;
		if (!(material_flags & MATERIAL_FLAG_TILEABLE_HORIZONTAL))
			material.TextureLayer[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		if (!(material_flags & MATERIAL_FLAG_TILEABLE_VERTICAL))
			material.TextureLayer[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
	}

	void applyMaterialOptionsWithShaders(video::SMaterial &material) const
	{
		material.BackfaceCulling = (material_flags & MATERIAL_FLAG_BACKFACE_CULLING) != 0;
		if (!(material_flags & MATERIAL_FLAG_TILEABLE_HORIZONTAL)) {
			material.TextureLayer[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
			material.TextureLayer[1].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		}
		if (!(material_flags & MATERIAL_FLAG_TILEABLE_VERTICAL)) {
			material.TextureLayer[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
			material.TextureLayer[1].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
		}
	}

	bool isTileable() const
	{
		return (material_flags & MATERIAL_FLAG_TILEABLE_HORIZONTAL)
			&& (material_flags & MATERIAL_FLAG_TILEABLE_VERTICAL);
	}

	explicit operator bool() const
	{
		return bool(get().texture_id);
	}

	explicit operator TileLayer() const
	{
		TileLayer result = get();
		result.color = color;
		result.material_flags = material_flags;
		return result;
	}

	const TileLayer *operator->() const
	{
		return &get();
	}

	bool operator==(const LayerRef &b) const
	{
		return
			material_flags == b.material_flags &&
			color == b.color &&
			get().texture_id == b->texture_id &&
			get().material_type == b->material_type &&
			get().scale == b->scale;
	}

	bool operator!=(const LayerRef &b) const
	{
		return !operator==(b);
	}
};

struct TileRef
{
	const TileSpec *tile = nullptr;
	video::SColor colors[MAX_TILE_LAYERS];
	u8 material_flags[MAX_TILE_LAYERS];
	u8 rotation = 0;
	u8 emissive_light = 0;

	TileRef() = default;

	TileRef(const TileSpec *tile) :
		tile(tile)
	{
		for (int k = 0; k < MAX_TILE_LAYERS; k++) {
			const TileLayer &layer = tile->layers[k];
			colors[k] = layer.color;
			material_flags[k] = layer.material_flags;
		}
	}

	TileRef(const TileSpec *tile, video::SColor color) :
		tile(tile)
	{
		for (int k = 0; k < MAX_TILE_LAYERS; k++) {
			const TileLayer &layer = tile->layers[k];
			colors[k] = layer.has_color ? layer.color : color;
			material_flags[k] = layer.material_flags;
		}
	}

	void setMaterialFlags(u8 set_flags, u8 clear_flags = 0)
	{
		u8 mask = ~clear_flags;
		for(u8 &mf : material_flags) {
			mf |= set_flags;
			mf &= mask;
		}
	}

	LayerRef getLayer(int layer = 0) const
	{
		assert((layer >= 0) && (layer < MAX_TILE_LAYERS));
		LayerRef ref;
		ref.tile = tile;
		ref.color = colors[layer];
		ref.material_flags = material_flags[layer];
		ref.rotation = rotation;
		ref.emissive_light = emissive_light;
		ref.layer = layer;
		return ref;
	}

	bool isTileable(const TileRef &b) const
	{
		if (rotation || b.rotation)
			return false;
		if (emissive_light != b.emissive_light)
			return false;
		for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
			LayerRef x = getLayer(layer);
			if (!x.isTileable())
				return false;
			if (x != b.getLayer(layer))
				return false;
		}
		return true;
	}

	const TileSpec *operator->() const
	{
		return tile;
	}

	LayerRef operator[](int layer) const
	{
		return getLayer(layer);
	}
};
