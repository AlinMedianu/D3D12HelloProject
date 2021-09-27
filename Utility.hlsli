#ifndef UTILITY
#define UTILITY

typedef float world;
typedef float2 texture2;
typedef float2 local2;
typedef float3 local3;
typedef float3 world3;
typedef float4 world4;
typedef float4 clip4;

namespace Light
{
    struct HemisphericAmbiental
    {
        float3 downColour;
        float3 colourDifference;
    };
    
    struct Directional
    {
        float3 colour;
        world3 normalizedInvertedDirection;
    };

    struct Point
    {
        float3 colour;
        float rangeReciprocal;
        world3 position;
    };

    struct Spot
    {
        float3 colour;
        float rangeReciprocal;
        world3 position;
        float cosOuterCone;
        world3 normalizedInvertedDirection;
        float cosInnerConeReciprocal;
    };

    struct Capsule
    {
        float3 colour;
        float rangeReciprocal;
        world3 segmentStartPosition;
        world segmentLength;
        world3 normalizedSegmentStartToSegmentEnd;
    };
}


float NormalizedToUnsignedNormalized(float number)
{
    return number * 0.5 + 0.5;
}

float3 NormalizedFromTo(float3 from, float3 to, out float fromToLength)
{
    float3 fromTo = to - from;
    fromToLength = length(fromTo);
    return fromTo / fromToLength;
}

float3 ClosestPointOnSegmentFromPoint(float3 pointPosition, float3 segmentStartPosition, float3 normalizedSegmentStartToSegmentEnd, float segmentLength)
{
    float3 segmentStartPositionToPointPosition = pointPosition - segmentStartPosition;
    float distanceOnLine = dot(segmentStartPositionToPointPosition, normalizedSegmentStartToSegmentEnd);
    float distanceOnSegment = saturate(distanceOnLine / segmentLength) * segmentLength;
    return segmentStartPosition + normalizedSegmentStartToSegmentEnd * distanceOnSegment;
}

#endif