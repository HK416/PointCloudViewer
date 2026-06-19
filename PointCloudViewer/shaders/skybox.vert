#version 450
#extension GL_GOOGLE_include_directive : require

#include "global.glsl"

// ========== Output ==========
layout(location = 0) out vec3 outUVW;

// ========== Constants ==========
// 큐브의 36개 정점 위치 배열 (코드 생략, 일반적인 1x1x1 큐브 좌표)
const vec3 cubeVertices[36] = vec3[](
    vec3(-1.0,  1.0, -1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3( 1.0,  1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),

    vec3(-1.0, -1.0,  1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3(-1.0, -1.0,  1.0),

    vec3( 1.0, -1.0, -1.0),
    vec3( 1.0, -1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),

    vec3(-1.0, -1.0,  1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0, -1.0,  1.0),
    vec3(-1.0, -1.0,  1.0),

    vec3(-1.0,  1.0, -1.0),
    vec3( 1.0,  1.0, -1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3(-1.0,  1.0, -1.0),

    vec3(-1.0, -1.0, -1.0),
    vec3(-1.0, -1.0,  1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3(-1.0, -1.0,  1.0),
    vec3( 1.0, -1.0,  1.0)
);

void main() {
    outUVW = cubeVertices[gl_VertexIndex];
    
    // Skybox는 카메라의 이동(Translation)을 무시해야 하므로 View 매트릭스의 위치값 제거
    mat4 rotView = mat4(mat3(global.view)); 
    vec4 pos = global.proj * rotView * vec4(outUVW, 1.0);
    
    // 깊이 버퍼 최적화를 위해 z값을 w와 동일하게 설정 (항상 1.0의 깊이를 가짐)
    gl_Position = pos.xyww; 
}
