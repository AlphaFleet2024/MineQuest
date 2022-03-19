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

#include "pointedthing.h"

#include "gamedef.h"
#include "serialize.h"
#include "exceptions.h"
#include <sstream>

PointedThing::PointedThing(const v3pos_t &under, const v3pos_t &above,
	const v3pos_t &real_under, const v3opos_t &point, const v3pos_t &normal,
	u16 box_id, f32 distSq):
	type(POINTEDTHING_NODE),
	node_undersurface(under),
	node_abovesurface(above),
	node_real_undersurface(real_under),
	intersection_point(point),
	intersection_normal(normal),
	box_id(box_id),
	distanceSq(distSq)
{}

PointedThing::PointedThing(s16 id, const v3opos_t &point, const v3pos_t &normal,
	f32 distSq) :
	type(POINTEDTHING_OBJECT),
	object_id(id),
	intersection_point(point),
	intersection_normal(normal),
	distanceSq(distSq)
{}

std::string PointedThing::dump() const
{
	std::ostringstream os(std::ios::binary);
	switch (type) {
	case POINTEDTHING_NOTHING:
		os << "[nothing]";
		break;
	case POINTEDTHING_NODE:
	{
		const v3pos_t &u = node_undersurface;
		const v3pos_t &a = node_abovesurface;
		os << "[node under=" << u.X << "," << u.Y << "," << u.Z << " above="
			<< a.X << "," << a.Y << "," << a.Z << "]";
	}
		break;
	case POINTEDTHING_OBJECT:
		os << "[object " << object_id << "]";
		break;
	default:
		os << "[unknown PointedThing]";
	}
	return os.str();
}

void PointedThing::serialize(std::ostream &os, const u16 proto_ver) const
{

	const int version = proto_ver >= 41 ? 1 : 0;
	writeU8(os, version); // version
	writeU8(os, (u8)type);
	switch (type) {
	case POINTEDTHING_NOTHING:
		break;
	case POINTEDTHING_NODE:
		writeV3POS(os, node_undersurface, proto_ver);
		writeV3POS(os, node_abovesurface, proto_ver);
		break;
	case POINTEDTHING_OBJECT:
		writeS16(os, object_id);
		break;
	}
}

void PointedThing::deSerialize(std::istream &is)
{
	int version = readU8(is);
	if (version != 0 && version != 1) throw SerializationError(
			"unsupported PointedThing version");
	type = (PointedThingType) readU8(is);
	switch (type) {
	case POINTEDTHING_NOTHING:
		break;
	case POINTEDTHING_NODE:
		node_undersurface = readV3POS(is, version >= 1 ? 41 : 40);
		node_abovesurface = readV3POS(is, version >= 1 ? 41 : 40);
		break;
	case POINTEDTHING_OBJECT:
		object_id = readS16(is);
		break;
	default:
		throw SerializationError("unsupported PointedThingType");
	}
}

bool PointedThing::operator==(const PointedThing &pt2) const
{
	if (type != pt2.type)
	{
		return false;
	}
	if (type == POINTEDTHING_NODE)
	{
		if ((node_undersurface != pt2.node_undersurface)
				|| (node_abovesurface != pt2.node_abovesurface)
				|| (node_real_undersurface != pt2.node_real_undersurface))
			return false;
	}
	else if (type == POINTEDTHING_OBJECT)
	{
		if (object_id != pt2.object_id)
			return false;
	}
	return true;
}

bool PointedThing::operator!=(const PointedThing &pt2) const
{
	return !(*this == pt2);
}
