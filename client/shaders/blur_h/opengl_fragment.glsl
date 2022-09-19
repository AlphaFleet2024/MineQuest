#define rendered texture0

uniform sampler2D rendered;
uniform vec2 texelSize0;
uniform lowp float bloomRadius = 3.0;

#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif

void main(void)
{
	// kernel distance and linear size
	lowp float n = 2. * bloomRadius + 1.;

	vec2 uv = varTexCoord.st - vec2(bloomRadius * texelSize0.x, 0.);
	vec4 color = vec4(0.);
	for (lowp float i = 0.; i < n; i++) {
		color += texture2D(rendered, uv).rgba * ((bloomRadius + 1.) - abs(i - bloomRadius));
		uv += vec2(texelSize0.x, 0.);
	}
	color /= (bloomRadius + 1.) * (bloomRadius + 1.);
	gl_FragColor = vec4(color.rgb, 1.0); // force full alpha to avoid holes in the image.
}
