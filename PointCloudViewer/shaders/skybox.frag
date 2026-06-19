#version 450
#extension GL_GOOGLE_include_directive : require

#include "global.glsl"

layout(location = 0) in vec3 inUVW;
layout(location = 0) out vec4 outColor;

// ========== Skybox Parameters ==========
layout(set = 1, binding = 0) uniform SkyboxParams {
	vec4 tintFactor;
} skybox;

layout(set = 1, binding = 1) uniform samplerCube cubeMap;

void main() {
	vec4 texColor = texture(cubeMap, inUVW);
	vec3 color = texColor.rgb * skybox.tintFactor.rgb;

	color = pow(color, vec3(1.0 / global.gamma));

	outColor = vec4(color, texColor.a * skybox.tintFactor.a);
}
