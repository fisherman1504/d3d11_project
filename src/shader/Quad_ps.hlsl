// Samplers.
SamplerState sampleStyle: register(s0); // Defines reconstruction filter etc.
SamplerState gBufferSampler : register(s1);			// Nearest Neighbor, clamp to edge.


// Textures for visualization.
Texture2D<float> gBufferDepth : register(t0);
Texture2D<float> dirLightViewDepth : register(t1);
Texture2D diffuseLighting : register(t2);
Texture2D specularLighting : register(t3);
Texture2D occlusionMap : register(t4);
Texture2D occlusionMapBlur : register(t5);




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


// Set in SponzaScene Render function.
cbuffer PS_TexVis_BUFFER : register(b1) {
	unsigned int textureIdx;
	float3 padding3;
};


// Entry point of shader.
float4 main(ps_in input) : SV_TARGET{
	// Show the requested texture.
	switch (textureIdx) {
	case 0:
		// gBuffer depth.
		float zNear = 3.0;
		float zFar = 300.0;
		float depth = gBufferDepth.Sample(gBufferSampler, input.TexCoords).r;

		depth = (2 * zNear) / (zFar + zNear - depth * (zFar - zNear));
		return float4(depth, depth, depth, 1.0);
		break;
	case 1:
		// Directional light camera depth.
		float ldepth = dirLightViewDepth.Sample(gBufferSampler, input.TexCoords).r;
		return float4(ldepth, ldepth, ldepth, 1.0);
		break;
	case 2:
		// Deferred: Point lights diffuse lighting.
		float3 diffuse = diffuseLighting.Sample(gBufferSampler, input.TexCoords).xyz;
		return float4(diffuse, 1.0);
		break;
	case 3:
		// Deferred: Point lights specular lighting.
		float3 specular = specularLighting.Sample(gBufferSampler, input.TexCoords).xyz;
		return float4(specular, 1.0);
		break;
	case 4:
		// SSAO occlusion map.
		float occlusion = occlusionMap.Sample(gBufferSampler, input.TexCoords).r;
		return float4(occlusion, occlusion, occlusion, 1.0);
		break;
	case 5:
		// Blurred (final) SSAO occlusion map.
		float occlusionB = occlusionMapBlur.Sample(gBufferSampler, input.TexCoords).r;
		return float4(occlusionB, occlusionB, occlusionB, 1.0);
		break;
	default:
		return float4(input.TexCoords,0.0,1.0);
	}
}