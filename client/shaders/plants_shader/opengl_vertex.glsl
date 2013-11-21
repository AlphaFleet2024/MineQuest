uniform mat4 mWorldViewProj;
uniform mat4 mInvWorld;
uniform mat4 mTransWorld;
uniform float dayNightRatio;
uniform float timeOfDay;

uniform vec3 eyePosition;

varying vec3 vPosition;
varying vec3 eyeVec;
varying vec3 tsEyeVec;

varying vec3 vertexWorldPos;
varying vec4 vertexViewPos;
varying vec4 vertexProjPos;

float smoothCurve( float x ) {  
  return x * x *( 3.0 - 2.0 * x );  
}  
float triangleWave( float x ) {  
  return abs( fract( x + 0.5 ) * 2.0 - 1.0 );  
}  
float smoothTriangleWave( float x ) {  
  return smoothCurve( triangleWave( x ) ) * 2.0 - 1.0;  
} 

void main(void)
{

	gl_TexCoord[0] = gl_MultiTexCoord0;
	vec4 pos = gl_Vertex;
	vec4 pos2 = mTransWorld*gl_Vertex;
	if (gl_TexCoord[0].y < 0.05) {
		pos.x += (smoothTriangleWave(timeOfDay * 200.0 + pos2.x * 0.1 + pos2.z * 0.1) * 2.0 - 1.0) * 0.8;
		pos.y -= (smoothTriangleWave(timeOfDay * 100.0 + pos2.x * -0.5 + pos2.z * -0.5) * 2.0 - 1.0) * 0.4;          
	}
	gl_Position = mWorldViewProj * pos;
	vPosition = (mWorldViewProj * gl_Vertex).xyz;

	vec3 normal,tangent,binormal; 
	normal = normalize(gl_NormalMatrix * gl_Normal);

	if (gl_Normal.x > 0.5) {
		//  1.0,  0.0,  0.0
		tangent  = normalize(gl_NormalMatrix * vec3( 0.0,  0.0, -1.0));
		binormal = normalize(gl_NormalMatrix * vec3( 0.0, -1.0,  0.0));
	} else if (gl_Normal.x < -0.5) {
		// -1.0,  0.0,  0.0
		tangent  = normalize(gl_NormalMatrix * vec3( 0.0,  0.0,  1.0));
		binormal = normalize(gl_NormalMatrix * vec3( 0.0, -1.0,  0.0));
	} else if (gl_Normal.y > 0.5) {
		//  0.0,  1.0,  0.0
		tangent  = normalize(gl_NormalMatrix * vec3( 1.0,  0.0,  0.0));
		binormal = normalize(gl_NormalMatrix * vec3( 0.0,  0.0,  1.0));
	} else if (gl_Normal.y < -0.5) {
		//  0.0, -1.0,  0.0
		tangent  = normalize(gl_NormalMatrix * vec3( 1.0,  0.0,  0.0));
		binormal = normalize(gl_NormalMatrix * vec3( 0.0,  0.0,  1.0));
	} else if (gl_Normal.z > 0.5) {
		//  0.0,  0.0,  1.0
		tangent  = normalize(gl_NormalMatrix * vec3( 1.0,  0.0,  0.0));
		binormal = normalize(gl_NormalMatrix * vec3( 0.0, -1.0,  0.0));
	} else if (gl_Normal.z < -0.5) {
		//  0.0,  0.0, -1.0
		tangent  = normalize(gl_NormalMatrix * vec3(-1.0,  0.0,  0.0));
		binormal = normalize(gl_NormalMatrix * vec3( 0.0, -1.0,  0.0));
	}
	
	mat3 tbnMatrix = mat3(tangent.x, binormal.x, normal.x,
                          tangent.y, binormal.y, normal.y,
                          tangent.z, binormal.z, normal.z);

	eyeVec = (gl_ModelViewMatrix * gl_Vertex).xyz;
	tsEyeVec = normalize(eyeVec * tbnMatrix);
	
	vec4 color;
	//color = vec4(1.0, 1.0, 1.0, 1.0);

	float day = gl_Color.r;
	float night = gl_Color.g;
	float light_source = gl_Color.b;

	/*color.r = mix(night, day, dayNightRatio);
	color.g = color.r;
	color.b = color.r;*/

	float rg = mix(night, day, dayNightRatio);
	rg += light_source * 2.5; // Make light sources brighter
	float b = rg;

	// Moonlight is blue
	b += (day - night) / 13.0;
	rg -= (day - night) / 13.0;

	// Emphase blue a bit in darker places
	// See C++ implementation in mapblock_mesh.cpp finalColorBlend()
	b += max(0.0, (1.0 - abs(b - 0.13)/0.17) * 0.025);

	// Artificial light is yellow-ish
	// See C++ implementation in mapblock_mesh.cpp finalColorBlend()
	rg += max(0.0, (1.0 - abs(rg - 0.85)/0.15) * 0.065);

	color.r = clamp(rg,0.0,1.0);
	color.g = clamp(rg,0.0,1.0);
	color.b = clamp(b,0.0,1.0);

	// Make sides and bottom darker than the top
	color = color * color; // SRGB -> Linear
	if(gl_Normal.y <= 0.5)
		color *= 0.6;
		//color *= 0.7;
	color = sqrt(color); // Linear -> SRGB

	color.a = gl_Color.a;

	gl_FrontColor = gl_BackColor = color;

}
