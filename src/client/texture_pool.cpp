/*
Minetest
Copyright (C) 2019 kilbith, Jean-Patrick Guerrero <jeanpatrick.guerrero@gmail.com>

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

#include "texture_pool.h"
#include "porting.h"

s32 TexturePool::addTexture(const std::string &name,
		const std::string &base_name, s32 frame_count,
		u64 frame_duration)
{
	s32 texture_idx = m_textures.size() + 1;
	m_textures.resize(texture_idx);
	m_textures[texture_idx - 1].set(name, base_name, frame_count, frame_duration);

	// Add this animation to the list
	m_animations.push_back(texture_idx - 1);

	return texture_idx;
}

s32 TexturePool::addTexture(const std::string &name)
{
	s32 texture_idx = m_textures.size() + 1;
	m_textures.resize(texture_idx);
	m_textures[texture_idx - 1].set(name);

	// Since this is a static texture, we *don't* add it to the list of
	// animations to update

	return texture_idx;
}

//! Get the texture index of the named texture, creating it if necessary
s32 TexturePool::getTextureIndex(const std::string &name)
{
	// Check if the texture was already loaded
	for (int i = 0, c = m_textures.size(); i < c; ++i)
		if (m_textures[i].getName() == name)
			return i + 1;

	// Expected format: "texture_name:frame_count,frame_duration"
	// If this format is not met, the string will be loaded as a normal texture

	std::string::size_type colon_position = name.find(':', 0);
	std::string::size_type comma_position = name.find(',', 0);

	if (comma_position > colon_position && comma_position < name.size()) {
		std::string base_name = name.substr(0, colon_position);
		s32 frame_count = std::stoi(name.substr(
			colon_position + 1, comma_position - colon_position - 1));
		s32 frame_duration = std::stoi(name.substr(comma_position + 1));

		if (base_name.size() > 0 && frame_count > 0 && frame_duration > 0)
			return addTexture(name, base_name, frame_count, frame_duration);
		else
			return addTexture(base_name);
	}

	return addTexture(name);
}

//! Get a handle to the named texture, creating it and setting texture_index if necessary
video::ITexture *TexturePool::getTexture(const std::string &name,
	ISimpleTextureSource *tsrc, s32 *texture_idx)
{
	if (name.size() == 0)
		return tsrc->getTexture(name);

	// If there's no texture id, we look it up or create a new one to pass back
	s32 idx;
	if (texture_idx == nullptr) {
		idx = getTextureIndex(name);
	} else if (*texture_idx == 0) {
		idx = getTextureIndex(name);
		*texture_idx = idx;
	} else {
		idx = *texture_idx;
	}

	return m_textures[idx - 1].getTexture(tsrc);
}

//! Advance all animations by the elapsed time
void TexturePool::step()
{
	u64 new_global_time = porting::getTimeMs();
	u64 step_duration;

	if (m_global_time == 0)
		step_duration = 0;
	else
		step_duration = new_global_time - m_global_time;

	m_global_time = new_global_time;

	for (int i = 0, c = m_animations.size(); i < c; ++i)
		m_textures[m_animations[i]].step(step_duration);
}
