uniform vec4 emissiveColor;

varying lowp vec4 varColor;

void main(void)
{
	gl_Position = mWorldViewProj * inVertexPosition;

#ifdef GL_ES
	vec4 color = inVertexColor.bgra;
#else
	vec4 color = inVertexColor;
#endif

	color *= emissiveColor;

	varColor = color;
}
