#version 460

layout(location = 0) in vec2 outTexCoord;
layout(location = 1) in vec3 mvVertexPos;
layout(location = 2) in vec3 outLight;
layout(location = 3) flat in vec3 normal;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D textureSampler;

layout(location = 5) uniform float contrast;

float lightVariation(vec3 normal) {
	vec3 directionalPart = vec3(0, contrast/2.0, contrast);
	float baseLighting = 1.0 - contrast;
	return baseLighting + dot(normal, directionalPart);
}

float ditherThresholds[16] = float[16] (
	1.0/17.0, 9.0/17.0, 3.0/17.0, 11.0/17.0,
	13.0/17.0, 5.0/17.0, 15.0/17.0, 7.0/17.0,
	4.0/17.0, 12.0/17.0, 2.0/17.0, 10.0/17.0,
	16.0/17.0, 8.0/17.0, 14.0/17.0, 6.0/17.0
);

ivec2 random1to2(int v) {
	ivec4 fac = ivec4(11248723, 105436839, 45399083, 5412951);
	int seed = v*fac.x ^ fac.y;
	return seed*fac.zw;
}

bool passDitherTest(float alpha) {
	ivec2 screenPos = ivec2(gl_FragCoord.xy);
	screenPos += random1to2(0);
	screenPos &= 3;
	return alpha > ditherThresholds[screenPos.x*4 + screenPos.y];
}

void main() {
	fragColor = texture(textureSampler, outTexCoord)*vec4(outLight*lightVariation(normal), 1);
	if(!passDitherTest(fragColor.a)) discard;
	fragColor.a = 1.0;
}
