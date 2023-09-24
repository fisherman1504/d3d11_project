
// Input of the pixel shader.
struct ps_in {
	float4 FragPos : SV_POSITION;		// Clip space.
};


float4 main(ps_in input) : SV_TARGET {
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}