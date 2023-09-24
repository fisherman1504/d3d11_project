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
	float3 VertexNormal : NORMAL;		// View space.
};

// ALWAYS set by a ModelClass object.
cbuffer VS_PER_MODEL_CONSTANT_BUFFER : register(b0) {
	float4x4 modelMat;
	float3x3 normalMat;
};

// Setup by a Scene object.
cbuffer VS_PER_FRAME_CONSTANT_BUFFER : register(b1) {
	float4x4 viewMat;
	float4x4 projMat;
	float3 viewPos;		// World space.
	float padding0;

	float4x4 lightViewMat;
	float4x4 lightProjMat;
};


// Entry point of shader.
ps_in main(vs_in input) {
	// Coordinate system transformation.
	float4 worldCoord = mul(float4(input.position, 1.0f), modelMat);
	float4 cameraCoord = mul(worldCoord, viewMat);
	float4 screenCoord = mul(cameraCoord, projMat);

	// Construct output to pixel shader stage.
	ps_in output;
	output.FragPos = screenCoord;			// Clip space.
	output.TexCoords = input.texCoord;
	output.VertexNormal = normalize(mul(input.normal, normalMat));	// View space.

	return output;
}