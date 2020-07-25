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

#include <algorithm>

#include "collisionquery.h"
#include "log.h"

// Match NodeDef.
const u16 CollisionQueryContext::testBitmask[] = {
		1,	// COLLISION_MAX_Y
		2,	// COLLISION_MIN_Y
		4,	// COLLISION_MIN_Z
		8, // COLLISION_MIN_X
		16,       // COLLISION_MAX_Z
		32,	// COLLISION_MAX_X
		64,     // COLLISION_FACE_X
		128,    // COLLISION_FACE_Y
		256,    // COLLISION_FACE_Z
		64 | 128 | 256, // COLLISION_FACE_XYZ
	};
const u16 CollisionQueryContext::setBitmask[] = {
		1 | 128,	// COLLISION_MAX_Y
		2 | 128,	// COLLISION_MIN_Y
		4 | 256,	// COLLISION_MIN_Z
		8 | 64, // COLLISION_MIN_X
		16 | 256,       // COLLISION_MAX_Z
		32 | 64,	// COLLISION_MAX_X
	};
const u16 CollisionQueryContext::unsetBitmask[] = {
		1 | 2 | 128,    // COLLISION_MAX_Y
		1 | 2 | 128,    // COLLISION_MIN_Y
		4 | 16 | 256,   // COLLISION_MIN_Z
		8 | 32 | 64,    // COLLISION_MIN_X
		4 | 16 | 256,   // COLLISION_MAX_Z
		8 | 32 | 64,    // COLLISION_MAX_X
	};
const CollisionFace CollisionQueryContext::opposingFace[] = {
		COLLISION_BOX_MIN_Y,
		COLLISION_BOX_MAX_Y,
		COLLISION_BOX_MAX_Z,
		COLLISION_BOX_MAX_X,
		COLLISION_BOX_MIN_Z,
		COLLISION_BOX_MIN_X,
	};

CollisionQueryContext::CollisionQueryContext(u16 ctx, aabb3f box, InvertedIndex *index, std::vector<Collision> *collisions) :
		m_ctx(ctx)
{
	// Store face offsets for the box.
	m_face_offset[COLLISION_FACE_MIN_X] = box.MinEdge.X;
	m_face_offset[COLLISION_FACE_MIN_Y] = box.MinEdge.Y;
	m_face_offset[COLLISION_FACE_MIN_Z] = box.MinEdge.Z;
	m_face_offset[COLLISION_FACE_MAX_X] = box.MaxEdge.X;
	m_face_offset[COLLISION_FACE_MAX_Y] = box.MaxEdge.Y;
	m_face_offset[COLLISION_FACE_MAX_Z] = box.MaxEdge.Z;

	// Search the InvertedIndex for boxes that overlap with this box
	// on any one dimension.
	// Criteria: box.min - maxwidth < collision.min < box.max
	// && box.min < collision.max < box.max + maxwidth
	v3f width = index->getMaxWidth();

	IndexListIteratorSet pos, neg;

	index->getInterval(COLLISION_FACE_MIN_X, box.MinEdge.X, box.MaxEdge.X + width.X, &pos);
	index->getInterval(COLLISION_FACE_MAX_X, box.MaxEdge.X, box.MaxEdge.X + 2 * width.X, &neg);
	IndexListIteratorDifference diff(pos.getUnion(), neg.getUnion());
	addIndexList(&diff);

	index->getInterval(COLLISION_FACE_MAX_X, box.MinEdge.X - width.X, box.MaxEdge.X, &pos);
	index->getInterval(COLLISION_FACE_MIN_X, box.MinEdge.X - width.X * 2, box.MinEdge.X, &neg);
	diff.restart(pos.getUnion(), neg.getUnion());
	addIndexList(&diff);

	index->getInterval(COLLISION_FACE_MIN_Y, box.MinEdge.Y, box.MaxEdge.Y + width.Y, &pos);
	index->getInterval(COLLISION_FACE_MAX_Y, box.MaxEdge.Y, box.MaxEdge.Y + 2 * width.Y, &neg);
	diff.restart(pos.getUnion(), neg.getUnion());
	addIndexList(&diff);

	index->getInterval(COLLISION_FACE_MAX_Y, box.MinEdge.Y - width.Y, box.MaxEdge.Y, &pos);
	index->getInterval(COLLISION_FACE_MIN_Y, box.MinEdge.Y - width.Y * 2, box.MinEdge.Y, &neg);
	diff.restart(pos.getUnion(), neg.getUnion());
	addIndexList(&diff);

	index->getInterval(COLLISION_FACE_MIN_Z, box.MinEdge.Z, box.MaxEdge.Z + width.Z, &pos);
	index->getInterval(COLLISION_FACE_MAX_Z, box.MaxEdge.Z, box.MaxEdge.Z + 2 * width.Z, &neg);
	diff.restart(pos.getUnion(), neg.getUnion());
	addIndexList(&diff);

	index->getInterval(COLLISION_FACE_MAX_Z, box.MinEdge.Z - width.Z, box.MaxEdge.Z, &pos);
	index->getInterval(COLLISION_FACE_MIN_Z, box.MinEdge.Z - width.Z * 2, box.MinEdge.Z, &neg);
	diff.restart(pos.getUnion(), neg.getUnion());
	addIndexList(&diff, collisions, ~0);
	// TODO: This will generate a MaxZ collision for every overlapping box.
	// Check to see if it should be replaced with a MinZ collision.
	// Add the correct X and Y collisions.
}

u32 CollisionQueryContext::addIndexList(IndexListIterator *index, std::vector<Collision> *collisions, u16 faces_init)
{
	u32 count = 0;

	if (index->hasNext())
		do
		{	
			u16 faces = faces_init;
			u32 id = index->peek();
			f32 offset;
			CollisionFace face = index->nextFace(&offset);
			

			if (face != COLLISION_FACE_NONE && m_active.find(id) == m_active.end())
				m_active.emplace(std::piecewise_construct, std::tuple<u32>(id), std::tuple<>());

			while (face != COLLISION_FACE_NONE)
			{
				faces |= setBitmask[face];
				m_active[id].valid_faces |= setBitmask[face];
				m_active[id].face_offset[face] = offset;
				
				face = index->nextFace(&offset);
			}

			if (collisions && (m_active[id].valid_faces & testBitmask[COLLISION_FACE_XYZ]) == testBitmask[COLLISION_FACE_XYZ])
				count += registerCollision(id, faces, m_active[id].face_offset, collisions);
		} while (index->forward());

	return count;
}

u32 CollisionQueryContext::registerCollision(u32 id, u16 faces, const f32 *offsets, std::vector<Collision> *collisions)
{
	u32 count = 0;

	if (faces & testBitmask[COLLISION_FACE_X])
		count += registerCollision(id, faces, offsets, collisions, COLLISION_FACE_MIN_X, COLLISION_FACE_MAX_X);
		
	if (faces & testBitmask[COLLISION_FACE_Y])
		count += registerCollision(id, faces, offsets, collisions, COLLISION_FACE_MIN_Y, COLLISION_FACE_MAX_Y);
		
	if (faces & testBitmask[COLLISION_FACE_Z])
		count += registerCollision(id, faces, offsets, collisions, COLLISION_FACE_MIN_Z, COLLISION_FACE_MAX_Z);

	return count;
}
		
u32 CollisionQueryContext::registerCollision(u32 id, u16 faces, const f32 *offsets, std::vector<Collision> *collisions, CollisionFace min, CollisionFace max)
{
	u32 count = 0;
	f32 min_off = offsets[min] - m_face_offset[min];
	f32 max_off = m_face_offset[max] - offsets[max];
	bool min_test = faces & testBitmask[min];
	bool max_test = faces & testBitmask[max];

	if (min_test && (!max_test || min_off >= max_off))
	{

		collisions->emplace_back(m_ctx, min, id, min_off, 0);
		count++;
	}
	if (max_test && (!min_test || max_off >= min_off))
	{
		collisions->emplace_back(m_ctx, max, id, max_off, 0);
		count++;
	}
	return count;
}

u32 CollisionQueryContext::subtractIndexList(IndexListIterator *index)
{
	u32 count = 0;
	if (index->hasNext())
		do
		{	
			u32 id = index->peek();
			CollisionFace face = index->nextFace();

			if (face != COLLISION_FACE_NONE && m_active.find(id) == m_active.end())
				continue;

			while (face != COLLISION_FACE_NONE)
			{
				if ((m_active[id].valid_faces & setBitmask[COLLISION_FACE_XYZ]) == setBitmask[COLLISION_FACE_XYZ])
					count++;

				m_active[id].valid_faces &= ~unsetBitmask[face];
				m_active[id].face_offset[face] = 0.f;
				m_active[id].face_offset[opposingFace[face]] = 0.f;
				
				face = index->nextFace();
			}

			if (!m_active[id].valid_faces)
				m_active.erase(id);
		} while (index->forward());

	return count;
}

