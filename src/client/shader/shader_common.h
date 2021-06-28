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

#include "irrlichttypes.h"
#include "irr_v2d.h"
#include "irr_v3d.h"

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>

#ifdef __ANDROID__
	#include <gl3.h>
#else
	#include <glcorearb.h>
#endif

// Replacement for .contains(y) which requires c++20
#define STL_CONTAINS(x,y) ( x .find( y ) != x .end() )

// Return x[y] if present, or z
// Similar to nil-or short circuit in Lua
#define STL_AT_OR(x,y,z) ( ( x .find( y ) != x .end() ) ? x .at( y ) : z )

const u32 SHADER_DONT_CARE = 0xFFFFFFFF;

enum class CompareTest : u32 {
	DontCare		= SHADER_DONT_CARE,
	Never			= GL_NEVER,
	Always			= GL_ALWAYS,
	Less			= GL_LESS,
	LessEqual		= GL_LEQUAL,
	Equal			= GL_EQUAL,
	GreaterEqual	= GL_GEQUAL,
	Greater			= GL_GREATER,
	NotEqual		= GL_NOTEQUAL,
};
enum class BlendOp : u32 {
	DontCare		= SHADER_DONT_CARE,
	Add				= GL_FUNC_ADD,
	Subtract		= GL_FUNC_SUBTRACT,
	RevSubtract		= GL_FUNC_REVERSE_SUBTRACT,
	Min				= GL_FUNC_MIN,
	Max				= GL_FUNC_MAX,
};
enum class BlendFactor : u32 {
	DontCare		= SHADER_DONT_CARE,
	Zero			= GL_ZERO,
	One				= GL_ONE,
	Source			= GL_SRC_COLOR,
	Dest			= GL_DST_COLOR,
	SourceAlpha		= GL_SRC_ALPHA,
	DestAlpha		= GL_DST_ALPHA,
	NegSourceAlpha	= GL_ONE_MINUS_SRC_ALPHA,
	NegDestAlpha	= GL_ONE_MINUS_DST_ALPHA,
	NegSource		= GL_ONE_MINUS_SRC_COLOR,
	NegDest			= GL_ONE_MINUS_DST_COLOR,
};
enum class StencilOp : u32 {
	DontCare 		= SHADER_DONT_CARE,
	Keep			= GL_KEEP,
	Zero			= GL_ZERO,
	Replace			= GL_REPLACE,
	Increment		= GL_INCR,
	IncrementWrap	= GL_INCR_WRAP,
	Decrement		= GL_DECR,
	DecrementWrap	= GL_DECR_WRAP,
	Invert			= GL_INVERT,
};

// Non-programmable state of a shader or material.
// This struct is simple enough to allow easy hashing and equality comparison.
struct FixedFunctionState {
	bool		useBlending;
	BlendFactor	srcBlend;
	BlendFactor	dstBlend;

	// Alpha clip/test.
	// Note that it must be emulated with discard on some obscure drivers,
	// and thus we have to add a variant for that.
	bool		alphaTest;

	// See GLES 2.0 reference 4.1.4 "Stencil Test"
	CompareTest stencilTest;
	StencilOp	stencilFail;
	StencilOp	stencilZFail;
	StencilOp	stencilPass;

	// Disable for passes that should never apply AA,
	// like deferred rendering data.
	bool		allowAntiAliasing = true;

	CompareTest	depthTest;
	bool		depthWrite = true; // Disable for transparent shaders.

	float		brightness;
};

std::unordered_map< u32, u32 > uniformTypeStrides = {
	{ GL_FLOAT, 		sizeof( GLfloat ) },
	{ GL_FLOAT_VEC2,	sizeof( GLfloat ) * 2 },
	{ GL_FLOAT_VEC3,	sizeof( GLfloat ) * 3 },
	{ GL_FLOAT_VEC4,	sizeof( GLfloat ) * 4 },
	{ GL_FLOAT_MAT2,	sizeof( GLfloat ) * 4 },
	{ GL_FLOAT_MAT3,	sizeof( GLfloat ) * 9 },
	{ GL_FLOAT_MAT4,	sizeof( GLfloat ) * 16 },
	{ GL_INT, 			sizeof( GLint ) },
	{ GL_INT_VEC2,		sizeof( GLint ) * 2 },
	{ GL_INT_VEC3,		sizeof( GLint ) * 3 },
	{ GL_INT_VEC4,		sizeof( GLint ) * 4 },
	{ GL_BOOL, 			sizeof( GLbool ) },
	{ GL_BOOL_VEC2,		sizeof( GLbool ) * 2 },
	{ GL_BOOL_VEC3,		sizeof( GLbool ) * 3 },
	{ GL_BOOL_VEC4,		sizeof( GLbool ) * 4 },
	{ GL_SAMPLER_2D,	sizeof( GLuint ) },
	{ GL_SAMPLER_3D,	sizeof( GLuint ) },
	{ GL_SAMPLER_CUBE,	sizeof( GLuint ) },
};
