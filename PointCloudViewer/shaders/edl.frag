#version 450

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 0, binding = 1) uniform sampler2D depthTexture;

layout(push_constant) uniform EDLParams {
	float screenWidth;
	float screenHeight;
	float edlStrength;
	float edlRadius;
	float nearPlane;
	float farPlane;
} params;

const int NUM_NEIGHBORS = 8;
const float PI = 3.14159265359;
const vec2 neighborOffsets[8] = vec2[](
	vec2( 1.000,  0.000), //   0°
	vec2( 0.707,  0.707), //  45°
	vec2( 0.000,  1.000), //  90°
	vec2(-0.707,  0.707), // 135°
	vec2(-1.000,  0.000), // 180°
	vec2(-0.707, -0.707), // 225°
	vec2( 0.000, -1.000), // 270°
	vec2( 0.707, -0.707)  // 315°
);

float linearizeDepth(float d) {
	return params.nearPlane * params.farPlane / (params.farPlane - d * (params.farPlane - params.nearPlane));
}

float edlResponse(float logDepthCenter, vec2 coord, vec2 offset) {
	float neighborDepth = texture(depthTexture, coord + offset).r;

	if (neighborDepth >= 1.0) return 0.0;

	float neighborLinear = linearizeDepth(neighborDepth);
	float logDepthNeighbor = log2(neighborLinear);

	return max(0.0, logDepthCenter - logDepthNeighbor);
}

void main() {
	vec4 color = texture(colorTexture, texCoord);
	float depth = texture(depthTexture, texCoord).r;

	// 배경 픽셀의 경우 EDL을 적용하지 않음
	if (depth >= 1.0) {
		outColor = color;
		return;
	}

	// 중심 픽셀의 로그 깊이
	float linearDepth = linearizeDepth(depth);
	float logDepthCenter = log2(linearDepth);

	// 픽셀 크기 (반경을 UV 공간으로 변환)
	vec2 pixelSize = vec2(1.0 / params.screenWidth, 1.0 / params.screenHeight);

	// 이웃 깊이 반응 누적
	float response = 0.0;
	for (int i = 0; i < NUM_NEIGHBORS; i++) {
		vec2 offset = neighborOffsets[i] * params.edlRadius * pixelSize;
		response += edlResponse(logDepthCenter, texCoord, offset);
	}
	response /= float(NUM_NEIGHBORS);

	// 셰이딩 펙터
	float shade = exp(-response * params.edlStrength);

	outColor = vec4(color.rgb * shade, color.a);
}
