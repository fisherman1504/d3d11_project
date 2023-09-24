// Samplers.
SamplerState sampleStyle: register(s0); // Defines reconstruction filter etc.

// Input of the pixel shader.
struct ps_in {
	float4 FragPos : SV_POSITION;		// Clip space.
	float2 TexCoords : TEXCOORD0;
	float3 VertexNormal : NORMAL;		// View space.

	// Taken from instance buffer.
	float4 Color: TEXCOORD1;
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
	return input.Color;
}