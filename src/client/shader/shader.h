/*
Minetest
Copyright (C) 2010-2021

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

#include "client/shader/shader_common.h"
#include "client/shader/shader_program.h"
#include "client/shader/shader_pass.h"

class Shader {
	std::unordered_map<std::string, u32> passMap;
	std::vector<ShaderPass> passes;

	u32 uniformCount;
	// Uniform indices by name, just for convenience.
	std::unordered_map<std::string, u32> uniformIndexMap;
	// Uniform names in order.
	std::vector<std::string> uniformNames;
	// Type of each uniform, as understood by GL
	std::vector<u32> uniformTypes;
	// Length of a uniform array returned by GL.
	std::vector<u32> uniformArrayLengths;
	// Size in bytes of a contiguous buffer that can store all the uniform state
	// required by this shader. It is a sum of all strides.
	size_t uniformBufferSize;
	// Offset of each uniform in the aforementioned buffer.
	std::vector<uintptr_t> uniformMemoryOffsets;

	/*
		The Location Matrix

		This is a 3-dimensional jagged array of uniform locations.
		The first two indices are pass index and variant key, and
		the third index is the uniform ID as understood by this class.

		To utilize this, you retrieve the appropriate row using the
		pass and variant, and then bring up this row to the material's
		list of uniform values. Then you iterate one by one, setting uniforms
		to the locations retrieved from the row, types known by the shader,
		and values known by the material, skipping wherever the location
		equals -1 (which means this particular Program did not contain
		this specific uniform after linking).
	*/
	std::vector<std::vector<std::vector<s32> > > locationMatrix;

	// Rebuild all uniform data.
	void BuildUniformData();

	// For force-enabling features globally
	u64 enableMask;
	// For force-disabling features globally
	u64 disableMask;

public:
	inline u32 GetUniformCount() {
		return uniformCount;
	}
	inline s32 GetUniformIndex( const std::string &name ) {
		return STL_AT_OR( uniformIndexMap, name, -1 );
	}
	inline size_t GetUniformBufferSize() { return uniformBufferSize; }
	inline ShaderProgram &GetProgram( u32 passId, u64 variant, u64 &outActualVariant ) {
		outActualVariant = (variant & disableMask ) | enableMask;
		return passes[passId].GetProgram( outActualVariant );
	}

	Shader( const std::unordered_map<std::string,PassSources> &sources );
};
