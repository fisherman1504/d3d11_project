// Samplers.
SamplerState sampleStyle: register(s0); // Defines reconstruction filter etc.

// Textures. TODO: Change to texture arrays in the future.
Texture2D texture_ambient : register(t0);	// RGB (R8G8B8A8_UNORM)
Texture2D texture_diffuse : register(t1);	// RGB (R8G8B8A8_UNORM)
Texture2D texture_specular : register(t2);	// scalar (R8_UNORM)

Texture2D texture_normal : register(t3);	// RGB
Texture2D texture_bump : register(t4);		// scalar (R8_UNORM)

Texture2D texture_dissolve : register(t5);	// TODO: Use.scalar
Texture2D texture_emissive : register(t6);	// TODO: Used for PBR


// Information from vertex shader.
struct ps_in {
	float4 FragPos : SV_POSITION;		// Clip space.
	float2 TexCoords : TEXCOORD;
	float3 VertexNormal : NORMAL;		// World space.

	float3x3 TBN : TEXCOORD5;			// Transform texture normal to world space.
};


// TODO: Stop wasting space.
//float matShininess;
//float matOpticalDensity;
//float matDissolveFactor;
//bool matColorViaTex;
struct ps_out {
	float2 gNormal			: SV_Target0;	// World space, 1 empty entry.
	float4 gDiffuseSpecular	: SV_Target1;	// Diffuse (RGB), Specular (A)
};


// ALWAYS set by current Mesh.
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


float2 OctWrap(float2 v) {
	return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}


// Octahedron-normal vectors.
float2 EncodeNormal(float3 n) {
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
	n.xy = n.xy * 0.5 + 0.5;
	return n.xy;
}


// Entry point of shader.
ps_out main(ps_in input) {
	float4 ambientColor;
	float4 diffuseColor;
	float4 specularColor;
	if (matColorViaTex) {
		// Sample from textures. TODO: Fix case when ambient is 0.
		diffuseColor = texture_diffuse.Sample(sampleStyle, input.TexCoords);
		ambientColor = diffuseColor; //texture_ambient.Sample(sampleStyle, input.TexCoords);
		specularColor = texture_specular.Sample(sampleStyle, input.TexCoords);
	} else {
		// Use per mesh information.
		ambientColor = float4(matAmbientColor, 1.0);
		diffuseColor = float4(matDiffuseColor, 1.0);
		specularColor = float4(matSpecularColor, 1.0);
	}


	// #############################################################################
	// Perform alpha testing (NOT BLENDING) in order to deal with transparency.
	// See https://www.youtube.com/watch?v=Dq8uNbT-o6I
	float alphaAmb = ambientColor.w;
	float alphaDiff = diffuseColor.w;
	float alphaSpec = specularColor.w;
	float alpha = alphaDiff;
	clip(alphaDiff < 0.1f ? -1 : 1);

	// #############################################################################
	// Transform texture normal from tangent space to world space.
	float3 mapNormal = texture_normal.Sample(sampleStyle, input.TexCoords);
	bool isDefaultNormalMap = all(mapNormal >= -0.01 && mapNormal <= 0.01);
	mapNormal = (mapNormal * 2.0 - 1.0);
	mapNormal = mul(mapNormal, input.TBN);
	float3 normal;
	if (isDefaultNormalMap) {
		// Use the original vertex normal.
		normal = input.VertexNormal;
	} else {
		// Perturb the vertex normal using the normal map.
		normal = mapNormal;
	}
	normal = normalize(normal);

	// Write into gBuffer.
	ps_out output;
	output.gNormal = EncodeNormal(normal);
	output.gDiffuseSpecular = float4(diffuseColor.xyz, specularColor.x);
	return output;
}