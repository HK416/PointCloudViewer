#ifndef GLOBAL_GLSL
#define GLOBAL_GLSL

// ========== Global Data ==========
layout(set = 0, binding = 0) uniform GlobalData {
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
	float pointSizeMultiplier;
	float pointSizeMin;
	float pointSizeMax;
	float gamma;
} global;

#endif // GLOBAL_GLSL
