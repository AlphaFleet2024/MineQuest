/*
Minetest
Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

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

#include <unordered_map>
#include <client/clientobject.h>
#include "irrlichttypes.h"

namespace mt {

template<typename T>
class ActiveObjectMgr {
public:
	ActiveObjectMgr() = default;
	~ActiveObjectMgr() = default;

	virtual void step(float dtime, std::function<void(T *)> &f) = 0;
	virtual void clear(bool force) = 0;
	virtual bool registerObject(T *obj) = 0;
	virtual void removeObject(u16 id) = 0;

	T * getActiveObject(u16 id)
	{
		typename std::unordered_map<u16, T *>::const_iterator n = m_active_objects.find(id);
		return (n != m_active_objects.end() ? n->second : NULL);
	}

	u16 getFreeId() const
	{
		// try to reuse id's as late as possible
		static u16 last_used_id = 0;
		u16 startid = last_used_id;
		for(;;) {
			last_used_id ++;
			if (isFreeId(last_used_id))
				return last_used_id;

			if (last_used_id == startid)
				return 0;
		}
	}

	bool isFreeId(const u16 id) const
	{
		return id != 0 && m_active_objects.find(id) == m_active_objects.end();

	}
protected:
	std::unordered_map<u16, T *> m_active_objects;
};

}