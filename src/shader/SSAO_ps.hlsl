// Samplers.
SamplerState sampleStyle: register(s0);				// Bilinear, wrap.
SamplerState gBufferSampler : register(s1);			// Nearest Neighbor, clamp to edge.
SamplerState tiledTextureSampler : register(s2);	// Nearest Neighbor, wrap.


// Textures.
Texture2D<float> gBufferDepth : register(t0);	// z-Buffer from gBuffer.
Texture2D gNormal : register(t1);				// Normal from gBuffer.
Texture2D noiseTexture : register(t2);			// Random vector noise texture (4x4).


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


// Sample positions.
cbuffer hemisphereKernelBuffer : register(b2) {
	float4 samples[64];	// [-1,1][-1,1][0,1]
}


// SSAO parameters.
cbuffer ssaoParameterBuffer : register(b3) {
	float radius;			// Size of sphere.
	float bias;
	int kernelSize;	// How many samples we take.
	bool useSSAO;
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
	// Compute the noise scale: Width/4 and Height/4.
	float2 noiseScale = float2((1.0/pixelSize.x)/4.0, (1.0 / pixelSize.y) / 4.0);

	// Sample from textures.
	float3 fragPos = reconstructWorldPos(input.TexCoords);
	float3 randomVec = noiseTexture.Sample(tiledTextureSampler, input.TexCoords* noiseScale).xyz;	// z-component is 0, x and y are -1 to 1
	float3 normal = normalize(DecodeNormal(gNormal.Sample(gBufferSampler, input.TexCoords)));

	// Compute TBN.
	float3 T = normalize(randomVec - normal * dot(randomVec, normal));
	float3 B = normalize(cross(normal, T));
	float3 N = normal;
	//if (dot(cross(N, T), B) < 0.0) {
	//	T = T * -1.0;
	//}
	float3x3 TBN = float3x3(T, B, N);

	// Compute how many samples in the sphere are being occluded by scene geometry.
	float occlusion = 0.0;
	float geometryDepth;
	float samplePosDist;
	float4 projectedPosition;
	for (int i = 0; i < kernelSize; ++i) {
		// Get sample position (world space).
		float3 samplePos = mul(samples[i].xyz, TBN); // Tangent space -> World space. 
		samplePos = fragPos + samplePos * radius;	// Point in the hemishphere (world space).

		// Project sample to the screen and get range 0-1, to sample from depth texture.
		projectedPosition = float4(samplePos, 1.0);
		projectedPosition = mul(mul(projectedPosition, viewMat), projMat);	// Project sample to camera screen.
		projectedPosition.xyz /= projectedPosition.w;						// Manual perspective divide since we work in a pixel shader.
		projectedPosition.xyz = projectedPosition.xyz * 0.5 + 0.5;		// transform to range 0.0 - 1.0  

		// TODO: Switch to depth buffer in the future.
		float3 sampleViaTex = reconstructWorldPos(float2(projectedPosition.x, 1 - projectedPosition.y));
		geometryDepth = length(sampleViaTex - viewPos);
		samplePosDist = length(samplePos - viewPos);

		// Update occlusion value.
		float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos - sampleViaTex));
		occlusion += (geometryDepth < samplePosDist + bias ? 1.0 : 0.0) * rangeCheck;
	}

	occlusion = (occlusion / float(kernelSize));
	return float4(occlusion,0.0,0.0,1.0);
}