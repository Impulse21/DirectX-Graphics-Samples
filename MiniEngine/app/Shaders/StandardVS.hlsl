// -- Parameters Begin ---

#include "StandardStructures.hlsli"

ConstantBuffer<DrawCall> DrawCallCB : register(b0);
ConstantBuffer<SceneData> SceneDataCB : register(b2);

// -- Parameters End ---

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;

};

struct VSOutput
{
    float4 positionWS : WPOSITION;
    float4 positionVS : VPOSITION;
    float3 normalVS : NORMAL;
    float3 normalWS : NORMAL1;
    float2 texCoord : TEXCOORD;
    float3 viewDirWS : TEXCOORD1;
    float4 position : SV_Position;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    output.positionWS = mul(DrawCallCB.Transform, float4(input.position, 1.0f));
    output.positionVS = mul(SceneDataCB.ViewMatrix, output.positionWS);
    output.position = mul(SceneDataCB.ProjectionMatrix, output.positionVS);
    
    output.normalWS = mul((float3x3) DrawCallCB.Transform, input.normal);  
    output.normalVS = mul((float3x3) SceneDataCB.ViewMatrix, output.normalWS);
    
    output.normalWS = normalize(output.normalWS);
    output.normalVS = normalize(output.normalVS);
    
    output.texCoord = input.texCoord;
    
    output.viewDirWS = SceneDataCB.CameriaPosition - output.positionWS.xyz;
    output.viewDirWS = normalize(output.viewDirWS);
    
    return output;
}