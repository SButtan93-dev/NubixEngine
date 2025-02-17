
#include "Lighting.hlsli"

cbuffer perFrame : register(b0)
{
    matrix InvViewProj;
    float3 CameraPosition;
    float WindowWidth;
    float WindowHeight;
}

cbuffer perLight : register(b1)
{
    Light ThisLight;
}

struct VertexToPixel
{
    float4 position : SV_POSITION;
    float3 viewRay : TEXCOORD0;
};

// Textures and such
Texture2D GBufferAlbedo : register(t0);
Texture2D GBufferNormals : register(t1);
Texture2D GBufferDepth : register(t2);
Texture2D GBufferMetalRough : register(t3);

float4 main(VertexToPixel input) : SV_TARGET
{
	// Load pixels from G-buffer (faster than sampling)
    float3 pixelIndex = float3(input.position.xy, 0);
    float3 surfaceColor = GBufferAlbedo.Load(pixelIndex).rgb;
    float3 normal = normalize(GBufferNormals.Load(pixelIndex).rgb * 2 - 1);
    float depth = GBufferDepth.Load(pixelIndex).r;
    float3 metalRough = GBufferMetalRough.Load(pixelIndex).rgb;

	// Calc UV of this pixel in screen space
    float2 pixelUV = input.position.xy / float2(WindowWidth, WindowHeight);

	// Calc world position from depth
    float3 worldPos = WorldSpaceFromDepth(depth, pixelUV, InvViewProj);

	// Handle lighting calculation (using regular albedo here due to energy conservation calculation inside PointLightPBR(),
	// so the deferred light buffer will already have the albedo taken into account - no need for a combine later)
    float metal = metalRough.r;
    float roughness = metalRough.g;
    float3 specColor = lerp(F0_NON_METAL.rrr, surfaceColor, metal);
    float3 color = PointLight(ThisLight, normal, worldPos, CameraPosition, roughness, surfaceColor, specColor.x);
    return float4(pow(color, 1.0f / 2.2f), 1.0f);
}