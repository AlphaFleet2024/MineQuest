/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013 Kahrl <kahrl@gmx.net>

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

#ifndef SHADER_HEADER
#define SHADER_HEADER

#include <IMaterialRendererServices.h>
#include "irrlichttypes_extrabloated.h"
#include "threads.h"
#include <string>

class IGameDef;

/*
	shader.{h,cpp}: Shader handling stuff.
*/

/*
	Gets the path to a shader by first checking if the file
	  name_of_shader/filename
	exists in shader_path and if not, using the data path.

	If not found, returns "".

	Utilizes a thread-safe cache.
*/
std::string getShaderPath(const std::string &name_of_shader,
		const std::string &filename);

struct ShaderInfo
{
	std::string name;
	video::E_MATERIAL_TYPE base_material;
	video::E_MATERIAL_TYPE material;
	u8 drawtype;
	u8 material_type;
	s32 user_data;

	ShaderInfo(): name(""), base_material(video::EMT_SOLID),
		material(video::EMT_SOLID),
		drawtype(0), material_type(0) {}
	virtual ~ShaderInfo() {}
};

/*
	Setter of constants for shaders
*/

namespace irr { namespace video {
	class IMaterialRendererServices;
} }

class IShaderConstantSetter
{
public:
	virtual ~IShaderConstantSetter(){};
	virtual void onSetConstants(video::IMaterialRendererServices *services,
			bool is_highlevel) = 0;
};

class IShaderConstantSetterFactory
{
public:
	virtual IShaderConstantSetter* create() = 0;
};

template <typename T>
class CachedVertexShaderSetting
{
	T m_sentValue;
	T m_value;
	const char *m_name;

public:

	CachedVertexShaderSetting(const char *name) :
		m_name(name)
	{}
	void set(const T &value)
	{
		m_value = value;
	}

	void send(video::IMaterialRendererServices *services)
	{
		if (m_sentValue != m_value) {
			services->setVertexShaderConstant(m_name, &m_value, 1);
			m_sentValue = m_value;
		}
	}

	void setAndSend(const T& value, video::IMaterialRendererServices *services)
	{
		set(value);
		send(services);
	}
};

template <typename T>
class CachedPixelShaderSetting
{
	T m_sentValue;
	T m_value;
	const char *m_name;

public:

	CachedPixelShaderSetting(const char *name) :
		m_name(name)
	{}
	void set(const T& value)
	{
		m_value = value;
	}

	void send(video::IMaterialRendererServices *services)
	{
		if (m_sentValue != m_value) {
			services->setPixelShaderConstant(m_name, &m_value, 1);
			m_sentValue = m_value;
		}
	}

	void setAndSend(const T& value, video::IMaterialRendererServices *services)
	{
		set(value);
		send(services);
	}
};

template <typename T, std::size_t size>
class CachedVertexShaderSetting<T[size]>
{
	T m_sentValue[size];
	T m_value[size];
	const char* m_name;
public:

	CachedVertexShaderSetting(const char *name) :
		m_name(name)
	{}
	void set(const T value[size])
	{
		std::copy(value, value + size, m_value);
	}

	void send(video::IMaterialRendererServices *services)
	{
		if (!std::equal(m_sentValue, m_sentValue + size, m_value)) {
			services->setVertexShaderConstant(m_name, m_value, size);
			std::copy(m_value, m_value + size, m_sentValue);
		}
	}

	void setAndSend(const T value[size], video::IMaterialRendererServices *services)
	{
		set(value);
		send(services);
	}
};

template <typename T, std::size_t size>
class CachedPixelShaderSetting<T[size]>
{
	T m_sentValue[size];
	T m_value[size];
	const char* m_name;
public:

	CachedPixelShaderSetting(const char *name) :
		m_name(name)
	{}
	void set(const T value[size])
	{
		std::copy(value, value + size, m_value);
	}

	void send(video::IMaterialRendererServices *services)
	{
		if (!std::equal(m_sentValue, m_sentValue + size, m_value)) {
			services->setPixelShaderConstant(m_name, m_value, size);
			std::copy(m_value, m_value + size, m_sentValue);
		}
	}

	void setAndSend(const T value[size], video::IMaterialRendererServices *services)
	{
		set(value);
		send(services);
	}
};

/*
	ShaderSource creates and caches shaders.
*/

class IShaderSource
{
public:
	IShaderSource(){}
	virtual ~IShaderSource(){}
	virtual u32 getShaderIdDirect(const std::string &name,
		const u8 material_type, const u8 drawtype){return 0;}
	virtual ShaderInfo getShaderInfo(u32 id){return ShaderInfo();}
	virtual u32 getShader(const std::string &name,
		const u8 material_type, const u8 drawtype){return 0;}
};

class IWritableShaderSource : public IShaderSource
{
public:
	IWritableShaderSource(){}
	virtual ~IWritableShaderSource(){}
	virtual u32 getShaderIdDirect(const std::string &name,
		const u8 material_type, const u8 drawtype){return 0;}
	virtual ShaderInfo getShaderInfo(u32 id){return ShaderInfo();}
	virtual u32 getShader(const std::string &name,
		const u8 material_type, const u8 drawtype){return 0;}

	virtual void processQueue()=0;
	virtual void insertSourceShader(const std::string &name_of_shader,
		const std::string &filename, const std::string &program)=0;
	virtual void rebuildShaders()=0;
	virtual void addShaderConstantSetterFactory(IShaderConstantSetterFactory *setter) = 0;
};

IWritableShaderSource* createShaderSource(IrrlichtDevice *device);

#endif
