
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
};

struct DrawCall
{
    matrix Transform;
    // ----------------------- (16 bit boundary)
};

#endif