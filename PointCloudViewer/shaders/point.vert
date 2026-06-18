#version 450

layout(location = 0) in vec3 pos; 
layout(location = 1) in vec3 color; 

layout(location = 0) out vec3 outColor;

layout(set = 0, binding = 0) uniform UniformBufferData {
	mat4 view;
	mat4 proj;
	vec3 cameraPos;
	float pointSizeMultiplier;
	float pointSizeMin;
	float pointSizeMax;
} ubo;

layout(push_constant) uniform Push { mat4 world; } p;

void main() { 
	vec4 worldPos = p.world * vec4(pos, 1.0);
	gl_Position = ubo.proj * ubo.view * worldPos;

	// 카메라와의 거리에 따라 포인트 크기 조절
	float dist = distance(ubo.cameraPos, worldPos.xyz);
	gl_PointSize = clamp(ubo.pointSizeMultiplier / max(dist, 1.0), ubo.pointSizeMin, ubo.pointSizeMax);

	outColor = color;
}
