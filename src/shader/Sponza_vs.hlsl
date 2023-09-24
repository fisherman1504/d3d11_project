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
	float4 FragPos : SV_POSITION;		// Clip space. Required.
	float2 TexCoords : TEXCOORD;
	float3 VertexNormal : NORMAL;		// World space.

	float3x3 TBN : TEXCOORD5;			// Transform texture normal to world space.
};

// ALWAYS setup by a ModelClass object.
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
	output.FragPos = screenCoord;			// Clip space. Required.
	output.TexCoords = input.texCoord;

	// Construct TBN matrix.
	//float3 T = normalize(mul(input.tangent, normalMat));
	//float3 B = normalize(mul(input.bitangent, normalMat));
	//float3 N = normalize(mul(input.normal, normalMat));
	float3 T = normalize(mul(float4(input.tangent,0.0), modelMat).xyz);
	float3 B = normalize(mul(float4(input.bitangent, 0.0), modelMat).xyz);
	float3 N = normalize(mul(float4(input.normal, 0.0), modelMat).xyz);

	// TBN must form a right handed coord system.
	// Some models have symetric UVs. Check and fix.
	if (dot(cross(N, T), B) < 0.0) {
		T = T * -1.0;
	}
	float3x3 TBN = float3x3(T, B, N);

	// Also pass additional information to PS. For visualization and debugging.
	output.VertexNormal = normalize(mul(float4(input.normal, 0.0), modelMat).xyz);
	output.TBN = TBN;

	return output;
}