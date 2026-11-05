#version 430

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec3 textureCoords;
layout(location = 1) flat in vec3 light;

layout(binding = 0) uniform highp sampler2DArray textureSampler;
layout(binding = 1) uniform highp sampler2DArray emissionTextureSampler;

void main() {
	vec4 texColor = texture(textureSampler, textureCoords);
	if(texColor.a < 0.5) discard;

	vec3 pixelLight = max(light, texture(emissionTextureSampler, textureCoords).r*4.0);
	fragColor = texColor*vec4(pixelLight, 1);
}
