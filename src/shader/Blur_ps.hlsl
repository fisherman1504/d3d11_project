// Samplers.
SamplerState sampleStyle: register(s0);				// Bilinear, wrap.
SamplerState gBufferSampler : register(s1);			// Nearest Neighbor, clamp to edge.

// Textures.
Texture2D occlusionMap : register(t0);		// Occlusion map.


// Information from vertex shader.
struct ps_in {
	float4 FragPos : SV_POSITION;	// Clip space.
	float2 TexCoords : TEXCOORD;
};


// ALWAYS set by current mesh.
cbuffer Material : register(b0) {
	float3 matAmbientColor;
	float matPadding0;

	float3 matDiffuseColor;
	float matPadding1;

	float3 matSpecularColor;
	float matPadding2;

	float matShininess;
	float matOpticalDensity;
	float matDissolveFactor;
	bool matColorViaTex;
	bool matPadding3;
	bool matPadding4;
	bool matPadding5;
}


// Information from scene.
cbuffer PS_CONSTANT_BUFFER : register(b1) {
	// Shading information information.
	float3 lightingScales;	// Weighting of phong terms.
	float shininessExp;

	// For visualization of normals in view space (matches sponza reference).
	float4x4 viewMat;

	// For SSAO.
	float4x4 projMat;

	// For rendering of light volumes.
	float4x4 invViewProjMat;

	// Other information.
	int drawMode;
	float3 viewPos;		// World space.
	float4 pixelSize;
};


// Entry point of shader.
float4 main(ps_in input) : SV_TARGET{
	// Sample 16 positions and average the occlusion values.
	float2 texelSize = pixelSize.xy;
	float result = 0.0;
	for (int x = -2; x < 2; ++x) {
		for (int y = -2; y < 2; ++y) {
			float2 offset = float2(float(x), float(y)) * texelSize;
			result += occlusionMap.Sample(gBufferSampler, input.TexCoords + offset).r;
		}
	}
	float avgOcclusion = result / (4.0 * 4.0);

	return float4(avgOcclusion,0.0,0.0,1.0);
}