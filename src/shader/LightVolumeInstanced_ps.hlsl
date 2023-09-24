// Samplers.
SamplerState sampleStyle: register(s0);		// Defines reconstruction filter etc.
SamplerState gBufferSampler : register(s1);	// Nearest Neighbor, clamp to edge.


// Textures.
Texture2D<float> gBufferDepth	: register(t0);
Texture2D gNormal				: register(t1);	// World space.


// Input of the pixel shader.
struct ps_in {
	float4 FragPos : SV_POSITION;
	float3 LightPos: TEXCOORD4;
	float LightRadius : TEXCOORD5;
	float3 LightColor: TEXCOORD6;
};


// Output of the pixel shader.
struct ps_out {
	float4 diffuseLighting	: SV_Target0;	// Diffuse lighting, 1 empty entry.
	float4 specularLighting	: SV_Target1;	// Specular lighting, 1 empty entry.
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
ps_out main(ps_in input){
	float2 texCoords = float2(input.FragPos.x * pixelSize.x,
		input.FragPos.y * pixelSize.y);

	// Reconstruct world position vida depth buffer.
	float3 fragPosWorld = reconstructWorldPos(texCoords);

	// Check if we are inside the volume. TODO: There is a weird error somewhere.
	// It's currently necessary to add 14 to the distance, otherwise the results
	// are not as expected.
	float dist = length(input.LightPos - fragPosWorld);
	float atten = 1.0 - clamp((dist + 15) / input.LightRadius, 0.0, 1.0);
	if (atten == 0.0) {
		discard;
	}

	// Compute attenuation in more detail according to table of
	// https://learnopengl.com/Lighting/Light-casters
	float lconst = 1.0;
	float llin = 0.14;	// Linear decrease with distance.
	float lquad = 0.07;	// Becomes large with higher distances. Quicker falloff.
	//atten = 1.0 / (lconst + llin * dist + lquad * (dist * dist));

	// Sample normal from gBuffer.
	float3 normal = normalize(DecodeNormal(gNormal.Sample(gBufferSampler, texCoords)));

	// Compute important directions.
	float3 incident = normalize(input.LightPos - fragPosWorld);
	float3 viewDir = normalize(viewPos - fragPosWorld);
	float3 halfDir = normalize(incident + viewDir);

	// Compute Phong shading terms.
	float lambert = clamp(dot(incident, normal), 0.0, 1.0);
	float rFactor = clamp(dot(halfDir, normal), 0.0, 1.0);
	float sFactor = pow(rFactor, shininessExp);

	// Construct output. Will be additively blended with other light volumes.
	ps_out output;
	output.diffuseLighting = float4(input.LightColor * lambert * atten, 1.0);
	output.specularLighting = float4(input.LightColor * sFactor * atten, 1.0);
	return output;
}