#include "Lighting.hlsli"

struct VertexInput
{
    local3 position : POSITION;
    local3 normal : NORMAL;
    local2 uv : UV;
};

struct PixelInput
{
    world3 position : POSITION;
    world3 normal : NORMAL;
    float2 uv : UV;
    clip4 screenPosition : SV_POSITION;
};

PixelInput Vertex(VertexInput input)
{
    PixelInput result;
    world4 worldPosition = mul(float4(input.position, 1), model);
    result.position = worldPosition.xyz;
    result.normal = mul(input.normal, (float3x3) model);
    result.uv = mul(float4(input.uv, 0, 1), textureTransform).xy;
    result.screenPosition = mul(worldPosition, viewProjection);
    return result;
}

float4 LitPixel(PixelInput input) : SV_TARGET
{
    float3 pixelLightColour = 0;
    float3 normalizedNormal = normalize(input.normal);
    #ifdef USE_HEMISPHERIC_AMBIENTAL_LIGHTING
    pixelLightColour += HemisphericAmbientalFactor(normalizedNormal.y);
    #endif
    #ifdef MAX_NUMBER_DIRECTIONAL_LIGHTS
    [unroll(8)]
    for (uint lightIndex = 0; lightIndex < MAX_NUMBER_DIRECTIONAL_LIGHTS; ++lightIndex)
    {
        pixelLightColour += CalculateLightColour(input.position, normalizedNormal, directionalLights[lightIndex]);
    }
    #endif
    #ifdef MAX_NUMBER_POINT_LIGHTS
    [unroll(8)]
    for (uint lightIndex = 0; lightIndex < MAX_NUMBER_POINT_LIGHTS; ++lightIndex)
    {
        pixelLightColour += CalculateLightColour(input.position, normalizedNormal, pointLights[lightIndex]);
    }
    #endif
    #ifdef MAX_NUMBER_SPOT_LIGHTS
    [unroll(8)]
    for (uint lightIndex = 0; lightIndex < MAX_NUMBER_SPOT_LIGHTS; ++lightIndex)
    {
        pixelLightColour += CalculateLightColour(input.position, normalizedNormal, spotLights[lightIndex]);
    }
    #endif
    #ifdef MAX_NUMBER_CAPSULE_LIGHTS
    [unroll(8)]
    for (uint lightIndex = 0; lightIndex < MAX_NUMBER_CAPSULE_LIGHTS; ++lightIndex)
    {
        pixelLightColour += CalculateLightColour(input.position, normalizedNormal, capsuleLights[lightIndex]);
    }
    #endif
    return saturate(float4(pixelLightColour, 1)) * diffuseColour * float4(textures[0].Sample(anisotropicWrap, input.uv).rgb, 1);
}

float4 ChannelStencilPixel(PixelInput input) : SV_TARGET
{
    uint stencil = channelStencil.Load(int3(input.screenPosition.xy, 0)).g;
    float4 colourMultiplier = float4(float((stencil & 1) != 0), float((stencil & 4) != 0), float((stencil & 16) != 0), 1);
    clip((float) stencil - 1);
    return diffuseColour;
}
