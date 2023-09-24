// Input of the vertex shader.
struct vs_in {
	float3 position : POSITION0;	// Object/model space.
	float2 texCoord : TEXCOORD0;
	float3 normal : NORMAL0;		// Object/model space.
	float3 tangent : TANGENT0;		// Object/model space.
	float3 bitangent : BINORMAL0;	// Object/model space. In HLSL called binormal.
};

// Input of the pixel shader.
struct ps_in {
	float4 FragPos : SV_POSITION;		// Clip space.
	float2 TexCoords : TEXCOORD;
};

// ALWAYS set by a ModelClass object.
cbuffer VS_PER_MODEL_CONSTANT_BUFFER : register(b0) {
	float4x4 modelMat;
	float3x3 normalMat;
};


// Set manually for SSAO.
cbuffer VS_MP_BUFFER : register(b1) {
	float4x4 ModelMat;
	float4x4 ProjectionMat;
};

// Entry point of shader.
ps_in main(vs_in input) {
	// Coordinate system transformation.
	float4 worldCoord = mul(float4(input.position, 1.0f), ModelMat);
	float4 screenCoord = mul(worldCoord, ProjectionMat);

	// Construct output to pixel shader stage.
	ps_in output;
	output.FragPos = screenCoord;			// Clip space.
	output.TexCoords = input.texCoord;

	return output;
}