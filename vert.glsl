#version 450

layout(location = 0) in vec3 pos; 
layout(location = 1) in vec3 color; 

layout(location = 0) out vec3 outColor;

layout(push_constant) uniform Push { mat4 mvp; } p;

void main() { 
	gl_Position = p.mvp * vec4(pos, 1.0); 
	gl_PointSize = 2.0; 

	outColor = color; 
}
