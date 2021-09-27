#ifndef GLOBALS
#define GLOBALS

#include "Utility.hlsli"

//#define USE_HEMISPHERIC_AMBIENTAL_LIGHTING
//#define MAX_NUMBER_DIRECTIONAL_LIGHTS 3
//#define MAX_NUMBER_POINT_LIGHTS 3
//#define MAX_NUMBER_SPOT_LIGHTS 3
//#define MAX_NUMBER_CAPSULE_LIGHTS 3

cbuffer PerScene : register(b0)
{
    matrix viewProjection;
    float3 cameraPosition;
#ifdef USE_HEMISPHERIC_AMBIENTAL_LIGHTING
    Light::HemisphericAmbiental ambientalLight;
#endif
#ifdef MAX_NUMBER_DIRECTIONAL_LIGHTS
    Light::Directional directionalLights[MAX_NUMBER_DIRECTIONAL_LIGHTS];
#endif
#ifdef MAX_NUMBER_POINT_LIGHTS
    Light::Point pointLights[MAX_NUMBER_POINT_LIGHTS];
#endif
#ifdef MAX_NUMBER_SPOT_LIGHTS
    Light::Spot spotLights[MAX_NUMBER_SPOT_LIGHTS];
#endif
#ifdef MAX_NUMBER_CAPSULE_LIGHTS
    Light::Capsule capsuleLights[MAX_NUMBER_CAPSULE_LIGHTS];
#endif
};
#endif

cbuffer PerModel : register(b1)
{
    matrix model;
    matrix textureTransform;
    float4 diffuseColour;
    float specularExponent;
    float specularIntensity;
};

Texture2D<uint2> channelStencil : register(t0);
SamplerState pointWrap : register(s0);
Texture2D textures[3] : register(t1);
SamplerState anisotropicWrap : register(s1);