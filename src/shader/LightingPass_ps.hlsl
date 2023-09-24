// Samplers.
SamplerState sampleStyle: register(s0); // Defines reconstruction filter etc.
SamplerComparisonState  depthMapStyle: register(s1); // For depth map.
SamplerState gBufferSampler : register(s2);			// Nearest Neighbor, clamp to edge.


// G-Buffer from the geometry pass.
Texture2D<float> dirLightDepth	: register(t0);
Texture2D<float> gBufferDepth	: register(t1);
Texture2D gNormal: register(t2);
Texture2D gDiffuseSpecular: register(t3);	// gDiffuseSpecular
Texture2D diffuseLightingTexture: register(t4);
Texture2D specularLightingTexture: register(t5);
Texture2D<float> occlusionMap : register(t6);


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


// Information for shadow mapping. Set by scene.
cbuffer PS_SHADOW_CONSTANT_BUFFER : register(b2) {
	float3 dirLightDir;	// World space.
	unsigned int shadowMapSize;
	float4x4 dirLightViewMat;	// TODO: Remove.
	float4x4 dirLightProjMat;	// TODO: Remove.
	unsigned int shadowType;	// Hard shadows, PCF, ...
	bool useShadows;
	unsigned int padding1;
	unsigned int padding2;
};

// SSAO parameters.
cbuffer ssaoParameterBuffer : register(b3) {
	float radius;			// Size of sphere.
	float bias;
	int kernelSize;	// How many samples we take.
	bool useSSAO;
}


// Checks if there is lighting (1) or shadows(0). Computes soft shadows by performing
// very basic PCF.
float ShadowCalculation(float4 lightFragPos, float3 lightDir, float3 normal) {
	// Compute depth from light camera for current fragment.
	float2 shadowTexCoords;
	shadowTexCoords.x = 0.5f + (lightFragPos.x / lightFragPos.w * 0.5f);
	shadowTexCoords.y = 0.5f - (lightFragPos.y / lightFragPos.w * 0.5f);
	float currentDepth = lightFragPos.z / lightFragPos.w;	// Shouldnt change anything for orthographic projection.

	// Avoid shadowed spots outside of the light frustum.
	if (currentDepth > 1.0) {
		return 1.0;
	}

	// Bias to decrease shadow acne.
	float bias = max(0.05 * (1.0 - dot(normal, -lightDir)), 0.005);

	// Compute lighting factor with the requested technique.
	float lighting = 0;
	if (shadowType == 0) {
		// Hard shadows without PCF.
		lighting = float(dirLightDepth.SampleCmpLevelZero(depthMapStyle,
			shadowTexCoords.xy, currentDepth - bias));
	} else {
		// Perform PCF.
		float texelSize = 1.0 / float(shadowMapSize);
		for (int x = -1; x <= 1; ++x) {
			for (int y = -1; y <= 1; ++y) {
				// Sample in 3x3 neighborhood.
				lighting += float(dirLightDepth.SampleCmpLevelZero(depthMapStyle,
					shadowTexCoords.xy + float2(x, y) * texelSize, currentDepth - bias));
			}
		}

		// Take average.
		lighting /= 9.0;
	}

	return lighting;
}


// Reconstruct world position via depth buffer.
float3 reconstructWorldPos(float2 uv) {
	float z = gBufferDepth.Sample(gBufferSampler, uv).r;
	float4 sPos = float4(uv * 2.0 - 1.0, z, 1.0);
	sPos.y *= -1.0;
	sPos = mul(sPos, invViewProjMat);
	return (sPos.xyz / sPos.w);
}


// Octahedron-normal vectors.
float3 DecodeNormal(float2 f) {
	f = f * 2.0 - 1.0;

	// https://twitter.com/Stubbesaurus/status/937994790553227264
	float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
	float t = saturate(-n.z);
	n.xy += n.xy >= 0.0 ? -t : t;
	return normalize(n);
}


// Entry point of shader.
float4 main(ps_in input) : SV_TARGET{	
	// Read from lighting pass textures.
	float3 light = diffuseLightingTexture.Sample(gBufferSampler, input.TexCoords).xyz;
	float3 specular = specularLightingTexture.Sample(gBufferSampler, input.TexCoords).xyz;
	
	// Read from gBuffer textures.
	float3 normal = normalize(DecodeNormal(gNormal.Sample(gBufferSampler, input.TexCoords)));
	float4 diffuseSpecular = gDiffuseSpecular.Sample(gBufferSampler, input.TexCoords);
	float3 diffuse = diffuseSpecular.xyz;
	float specMatFactor = diffuseSpecular.w;

	// Reconstruct world position vida depth buffer.
	float3 worldPosition = reconstructWorldPos(input.TexCoords);

	// Directional light shadows.
	float lightingPresence = 1.0;
	if (useShadows) {
		float4 lightFragPos = mul(mul(float4(worldPosition, 1.0), dirLightViewMat), dirLightProjMat);
		lightingPresence = ShadowCalculation(lightFragPos, normalize(dirLightDir), normalize(normal));
	}

	// Directional light lighting. Apply shadows to this lighting contribution only.
	float3 incident = normalize(-dirLightDir);
	float3 viewDir = normalize(viewPos - worldPosition);
	float3 halfDir = normalize(incident + viewDir);
	float lambert = clamp(dot(incident, normal), 0.0, 1.0);
	float rFactor = clamp(dot(halfDir, normal), 0.0, 1.0);
	float sFactor = pow(rFactor, shininessExp);
	light += float3(1.0, 1.0, 1.0) * lambert * lightingPresence;
	specular += float3(1.0, 1.0, 1.0) * sFactor * lightingPresence;

	// Gather occlusion info from SSAO occlusion map.
	float occlusion = 0.0;
	if (useSSAO) {
		occlusion = occlusionMap.Sample(gBufferSampler, input.TexCoords).r;
	}
	 
	// Compute final fragment color.
	float4 fragColor;
	fragColor.xyz = diffuse * lightingScales.x * (1.0-occlusion); // ambient
	fragColor.xyz += diffuse * light * lightingScales.y; // lambert
	fragColor.xyz += specular * lightingScales.z * specMatFactor; // Specular
	fragColor.a = 1.0;

	// Show requested output.
	switch (drawMode) {
	case 0:
		// Phong shading.
		return fragColor;
		break;
	case 1:
		// Ambient contribution.
		return float4(diffuse * lightingScales.x, 1.0);
		break;
	case 2:
		// Diffuse contribution.
		return float4(diffuse * light * lightingScales.y, 1.0);
		break;
	case 3:
		// Specular contribution.
		return float4(specular * lightingScales.z * specMatFactor, 1.0);
		break;
	case 4:
		// Visualize the normal that is being used in VIEW space. 
		float3 normalVS = mul(float4(normal, 0.0), viewMat).xyz;	// Transform to view space.
		float4 normalColor = float4(normalVS, 1.0);
		return normalColor;
		break;
	case 5:
		// World space position.
		return float4(worldPosition, 1.0);
		break;
	case 6:
		// Without model colors.
		float4 colorFree;
		colorFree.xyz = lightingScales.x * (1.0 - occlusion); // ambient
		colorFree.xyz += light * lightingScales.y; // lambert
		colorFree.xyz += specular * lightingScales.z * specMatFactor; // Specular
		colorFree.a = 1.0;
		return colorFree;
		break;
	default:
		return fragColor;
	}
}