#version 450

layout(location = 0) in vec3 pos; 
layout(location = 1) in vec3 color; 

layout(location = 0) out vec3 outColor;

layout(set = 0, binding = 0) uniform UniformBufferData {
	float pointSize;
	float minZ;
	float maxZ;
	int viewMode;
} ubo;

layout(push_constant) uniform Push { mat4 mvp; } p;

vec3 getHeatmapColor(float t) {
	t = clamp(t, 0.0, 1.0);
	float r = clamp(2.0 * t - 1.0, 0.0, 1.0);
	float g = 1.0 - 2.0 * abs(t - 0.5);
	float b = clamp(1.0 - 2.0 * t, 0.0, 1.0);
	return vec3(r, g, b);
}

void main() { 
	gl_Position = p.mvp * vec4(pos, 1.0); 
	gl_PointSize = ubo.pointSize;
	
	if (ubo.viewMode == 0) {
		outColor = color;
	} else {
		float t = (pos.z - ubo.minZ) / (ubo.maxZ - ubo.minZ + 0.0001);
		outColor = getHeatmapColor(t);
	}
}
