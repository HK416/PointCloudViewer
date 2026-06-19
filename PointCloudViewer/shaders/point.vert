#version 450
#extension GL_GOOGLE_include_directive : require

#include "global.glsl"

// ========== Input ==========
layout(location = 0) in vec3 pos; 
layout(location = 1) in vec3 color; 

// ========== Output ==========
layout(location = 0) out vec3 outColor;

// ========== Push Constants ===========
layout(push_constant) uniform Push { mat4 world; } p;

void main() { 
	vec4 worldPos = p.world * vec4(pos, 1.0);
	gl_Position = global.proj * global.view * worldPos;

	// 카메라와의 거리에 따라 포인트 크기 조절
	float dist = distance(global.cameraPos, worldPos.xyz);
	gl_PointSize = clamp(global.pointSizeMultiplier / max(dist, 1.0), global.pointSizeMin, global.pointSizeMax);

	outColor = color;
}
