uniform vec3 eyePosition;

// The cameraOffset is the current center of the visible world.
uniform vec3 cameraOffset;
uniform float animationTimer;
uniform float horizon;

varying vec3 vNormal;
varying vec3 vPosition;
varying vec3 worldPosition;
varying lowp vec4 varColor;
varying mediump vec2 varTexCoord;

varying vec3 eyeVec;
varying float vIDiff;

const float e = 2.718281828459;
const float BS = 10.0;

float directional_ambient(vec3 normal)
{
	vec3 v = normal * normal;

	if (normal.y < 0.0)
		return dot(v, vec3(0.670820, 0.447213, 0.836660));

	return dot(v, vec3(0.670820, 1.000000, 0.836660));
}

void main(void)
{
	varTexCoord = (mTexture * inTexCoord0).st;
	worldPosition = (mWorld * inVertexPosition).xyz;
	vec4 pos = mWorld * inVertexPosition;

#if ENABLE_CURVED_SURFACE
	vec2 xz_pos = eyePosition.xz - cameraOffset.xz - worldPosition.xz;
	pos.y -= dot(xz_pos, xz_pos) / horizon / horizon;
#endif
	gl_Position = mProj * mView * pos;
	vPosition = gl_Position.xyz;
	vNormal = inVertexNormal;
	eyeVec = -(mView * pos).xyz;

#if (MATERIAL_TYPE == TILE_MATERIAL_PLAIN) || (MATERIAL_TYPE == TILE_MATERIAL_PLAIN_ALPHA)
	vIDiff = 1.0;
#else
	// This is intentional comparison with zero without any margin.
	// If normal is not equal to zero exactly, then we assume it's a valid, just not normalized vector
	vIDiff = length(inVertexNormal) == 0.0
		? 1.0
		: directional_ambient(normalize(inVertexNormal));
#endif

	varColor = inVertexColor;
}
