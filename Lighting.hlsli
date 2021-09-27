#ifndef LIGHTING
#define LIGHTING

#include "Globals.hlsli"
 #ifdef USE_HEMISPHERIC_AMBIENTAL_LIGHTING
float3 HemisphericAmbientalFactor(float normalizedUpComponent)
{
    float unsignedNormalizedUpComponent = NormalizedToUnsignedNormalized(normalizedUpComponent);
    return mad(ambientalLight.colourDifference, unsignedNormalizedUpComponent, ambientalLight.downColour);
}
#endif
float3 CalculateDiffuseColour(float3 normalizedContactSurfaceNormal, float3 invertedLightDirection, float3 lightColour)
{
    float brightness = saturate(dot(invertedLightDirection, normalizedContactSurfaceNormal));
    return lightColour * brightness;
}

float3 CalculateSpecularColour(float3 contactPoint, float3 normalizedContactSurfaceNormal, float3 invertedLightDirection, float3 lightColour)
{
    float3 contactPointToCamera = normalize(cameraPosition - contactPoint);
    float3 halfWayVector = normalize(contactPointToCamera + invertedLightDirection);
    float brightness = saturate(dot(halfWayVector, normalizedContactSurfaceNormal));
    return lightColour * pow(brightness, specularExponent) * specularIntensity;
}

float CalculateSquaredAttenuation(float distanceToLightSource, float lightRangeReciprocal)
{
    float attenuation = 1 - saturate(distanceToLightSource * lightRangeReciprocal);
    return pow(attenuation, 2);
}

float CalculateSquaredConeAttenuation(float3 contactPointToLightSource, float3 invertedConeDirection, float cosOuterCone, float cosInnerConeReciprocal)
{
    float factor = dot(contactPointToLightSource, invertedConeDirection);
    float coneAttenuation = saturate((factor - cosOuterCone) * cosInnerConeReciprocal);
    return pow(coneAttenuation, 2);
}

float3 CalculateLightColour(float3 contactPoint, float3 normalizedContactSurfaceNormal, Light::Directional light)
{
    float3 diffuseColour = CalculateDiffuseColour(normalizedContactSurfaceNormal, light.normalizedInvertedDirection, light.colour);
    float3 specularColour = CalculateSpecularColour(contactPoint, normalizedContactSurfaceNormal, light.normalizedInvertedDirection, light.colour);
    return diffuseColour + specularColour;
}

float3 CalculateLightColour(float3 contactPoint, float3 normalizedContactSurfaceNormal, Light::Point light)
{
    float distanceFromContactPointToLightSource;
    float3 contactPointToLightSource = NormalizedFromTo(contactPoint, light.position, distanceFromContactPointToLightSource);
    float3 diffuseColour = CalculateDiffuseColour(normalizedContactSurfaceNormal, contactPointToLightSource, light.colour);
    float3 specularColour = CalculateSpecularColour(contactPoint, normalizedContactSurfaceNormal, contactPointToLightSource, light.colour);
    float attenuation = CalculateSquaredAttenuation(distanceFromContactPointToLightSource, light.rangeReciprocal);
    return (diffuseColour + specularColour) * attenuation;
}

float3 CalculateLightColour(float3 contactPoint, float3 normalizedContactSurfaceNormal, Light::Spot light)
{
    float distanceFromContactPointToLightSource;
    float3 contactPointToLightSource = NormalizedFromTo(contactPoint, light.position, distanceFromContactPointToLightSource);
    float3 diffuseColour = CalculateDiffuseColour(normalizedContactSurfaceNormal, contactPointToLightSource, light.colour);
    float3 specularColour = CalculateSpecularColour(contactPoint, normalizedContactSurfaceNormal, contactPointToLightSource, light.colour);
    float attenuation = CalculateSquaredAttenuation(distanceFromContactPointToLightSource, light.rangeReciprocal);
    float coneAttenuation = CalculateSquaredConeAttenuation(contactPointToLightSource, light.normalizedInvertedDirection, light.cosOuterCone, light.cosInnerConeReciprocal);
    return (diffuseColour + specularColour) * attenuation * coneAttenuation;
}

float3 CalculateLightColour(float3 contactPoint, float3 normalizedContactSurfaceNormal, Light::Capsule light)
{
    float3 lightSource = ClosestPointOnSegmentFromPoint(contactPoint, light.segmentStartPosition, light.normalizedSegmentStartToSegmentEnd, light.segmentLength);
    float distanceFromContactPointToLightSource;
    float3 contactPointToLightSource = NormalizedFromTo(contactPoint, lightSource, distanceFromContactPointToLightSource);
    float3 diffuseColour = CalculateDiffuseColour(normalizedContactSurfaceNormal, contactPointToLightSource, light.colour);
    float3 specularColour = CalculateSpecularColour(contactPoint, normalizedContactSurfaceNormal, contactPointToLightSource, light.colour);
    float attenuation = CalculateSquaredAttenuation(distanceFromContactPointToLightSource, light.rangeReciprocal);
    return (diffuseColour + specularColour) * attenuation;
}
#endif