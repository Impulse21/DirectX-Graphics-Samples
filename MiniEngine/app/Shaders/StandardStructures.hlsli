
#ifndef __STANDARD_STRUCTURES_HLSLI__
#define __STANDARD_STRUCTURES_HLSLI__

struct SceneData
{
    // -- Matrices ---
    matrix ProjectionMatrix;
    // ----------------------- (16 bit boundary)
	
    matrix ViewMatrix;
    // ----------------------- (16 bit boundary)
    
    // -- Camera Data ---
    float3 CameriaPosition;
    // ----------------------- (16 bit boundary)
    
    uint NumOmniLights;

    uint NumSpotLights;
    
    float padding[2];
    
    // ----------------------- (16 bit boundary)
};

struct DrawCall
{    
    matrix Transform;
    uint Flags;
};

#define LIGHT_OMNI 0
#define LIGHT_SPOT 1

struct LightData
{
    float3 Position;
    float RadiusSq;
    float3 Colour;
    
    uint Type;
    float3 ConeDirection;
    float3 ConeAngles;
};
#endif