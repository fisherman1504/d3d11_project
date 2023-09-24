// First vertex buffer slot.
struct vs_in0 {
	float3 position : POSITION0;	// Object/model space.
	float2 texCoord : TEXCOORD0;
	float3 normal : NORMAL0;		// Object/model space.
	float3 tangent : TANGENT0;		// Object/model space.
	float3 bitangent : BINORMAL0;	// Object/model space. In HLSL called binormal.
};

// Second vertex buffer slot (instance buffer).
struct vs_in1 {
	// From Instance buffer.
	float3 instancePosition : TEXCOORD1;
	float3 instanceScale	: TEXCOORD2;
	float3 instanceColor	: TEXCOORD3;
};

// Input of the pixel shader.
struct ps_in {
	float4 FragPos : SV_POSITION;		// Clip space.
	float2 TexCoords : TEXCOORD0;
	float3 VertexNormal : NORMAL;		// View space.

	// Taken from instance buffer.
	float4 Color: TEXCOORD1;
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
ps_in main(vs_in0 input0, vs_in1 input1) {
	// Take instance buffer data to do the transformation into world space.
	float3 scaledPos = input0.position * 3.0;
	float3 translatedPos = scaledPos + input1.instancePosition.xyz;
	float4 worldCoord = float4(translatedPos, 1.0);

	// Do the remaining transformation.s
	float4 cameraCoord = mul(worldCoord, viewMat);
	float4 screenCoord = mul(cameraCoord, projMat);

	// Construct output to pixel shader stage.
	ps_in output;
	output.FragPos = screenCoord;			// Clip space.
	output.TexCoords = input0.texCoord;
	output.VertexNormal = normalize(mul(input0.normal, normalMat));	// View space.
	output.Color = float4(input1.instanceColor,1.0);

	return output;
}