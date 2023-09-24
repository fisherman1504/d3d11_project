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
};


// Setup by a ModelClass object.
cbuffer VS_PER_MODEL_CONSTANT_BUFFER : register(b0) {
	float4x4 modelMat;
	float3x3 normalMat;
};

// Setup by a Scene object.
cbuffer VS_SHADOW_CONSTANT_BUFFER : register(b1) {
	float4x4 lightViewMat;
	float4x4 lightProjMat;
};




ps_in main(vs_in input){
	// Transform to space of light camera and project.
	float4 worldCoord = mul(float4(input.position, 1.0f), modelMat);
	float4 cameraCoord = mul(worldCoord, lightViewMat);
	float4 screenCoord = mul(cameraCoord, lightProjMat);

	// Output.
	ps_in output;
	output.FragPos = screenCoord;
	return output;
}