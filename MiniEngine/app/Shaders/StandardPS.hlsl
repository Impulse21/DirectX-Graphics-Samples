#include "StandardStructures.hlsli"

#include "BRDFFunctions.hlsli"


// Constant normal incidence Fresnel factor for all dielectrics.
static const float3 Fdielectric = 0.04;

static const float Epsilon = 0.00001;

float CalculateAttenuation(float c, float l, float q, float d)
{
    return 1.0f / (c + 1.0f * d + q * d * d);
}

struct Material
{
    float4 Albedo;
    
    float Metalness;
    
    float Roughness;
    
    float AmbientOcclusion;
    
    float3 Emissive;
    
    float _padding[2];
};

struct DirectionLight
{
    float4 Direction;
    // ----------------------- (16 bit boundary)
    
    float3 Colour;
    float _padding;
};

// -- Constant Buffers ---
ConstantBuffer<DrawCall> DrawCallCB : register(b0);
ConstantBuffer<Material> MaterialCB : register(b1);
ConstantBuffer<SceneData> SceneDataCB : register(b2);
ConstantBuffer<DirectionLight> DirectionLightsCB : register(b3);
StructuredBuffer<LightData> OmniLightData : register(t1);

// -- Textures ---
// Texture2D AlbedoTexture : register(t0);
// Texture2D NormalTexture : register(t1);

// R = Rougness, g = Metalness, b = AO;
// Texture2D MaterialTexture : register(t1);

// -- Samplers ---
SamplerState LinearRepeatSampler : register(s0);

// -- Pixel input Definition ---
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

// -- Calculate specular BRDF ---
float3 CalculateSpecularBrdf(
    float3 L,
    float3 radiance,
    float3 N,
    float3 V,
    float NdotV,
    float3 F0,
    float3 albedo,
    float roughness,
    float metalness,
    float ao)
{
    // Halfway vector - the halfway vector from the view vector to the light vector.
    float3 H = normalize(V + L);
        
    float HdotV = saturate(dot(H, V));
    float NdotL = saturate(dot(N, L));
      
        
    // Calculate Cooks tolerences terms
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(HdotV, F0);
        
	// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
	// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
	// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
    // float3 kS = F;
    //float3 kD = lerp(float3(1.0f, 1.0f, 1.0f) - kS, float3(0.0f, 0.0f, 0.0f), metalness);
    float3 kS = F;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
    kD *= 1.0 - metalness;
        
    // Lambert diffuse BRDF.
	// We don't scale by 1/PI for lighting & material units to be more convenient.
	// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    float3 diffuseBRDF = kD * albedo;
        
    // Cook-Torrance specular micofacet BRDF
    // float3 specualrBRDF = (F * NDF * G) / max(Epsilon, 4.0 * NdotV * NdotL);
    float3 specualrBRDF = (F * NDF * G) / max(Epsilon, 4.0 * NdotV * NdotL);
    return (diffuseBRDF + specualrBRDF) * radiance * NdotL;
}

float4 main(VSOutput input) : SV_TARGET
{
    // -- Determine material information ---
    float3 albedo = MaterialCB.Albedo.xyz;
    //albedo += saturate(AlbedoTexture.Sample(LinearRepeatSampler, input.texCoord).xyz);
   
    // float4 materialTextureData = MaterialTexture.Sample(LinearRepeatSampler, input.texCoord);
    
    float roughness = MaterialCB.Roughness;
    //roughness += materialTextureData.r;
    //roughness = saturate(roughness);
    
    float metalness = MaterialCB.Metalness;
    // metalness += materialTextureData.g;
    // metalness = saturate(metalness);
    
    float ao = MaterialCB.AmbientOcclusion;
    //ao += materialTextureData.b;
    //ao = saturate(ao);
    // -- End Material Information ----
    
    float3 N = normalize(input.normalWS);
    float3 V = normalize(input.viewDirWS);
    float NdotV = saturate(dot(N, V));

    // Fresnel reflectance at normal incidence ( for metals use albedo colour)
    //   - In PBR metalness workflows we make the simpligying assumption that most dielectric sufraces
    //     look visually correct with a constant 0.04.
    float3 F0 = lerp(Fdielectric, albedo, metalness);
    
    // Specular reflection vector.
    float3 Lr = 2.0 * NdotV * N - V;
    
    // Outgoing radiance calcuation for analytical lights
    float3 directLighting = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < SceneDataCB.NumOmniLights; i++)
    {
        LightData lightData = OmniLightData[i];
        
        float distance = length(lightData.Position - input.positionWS.xyz);
        
        // This is from the Mini Engine Example
        float3 lightDirection = lightData.Position - input.positionWS.xyz;
        float lightDistanceSq = dot(lightDirection, lightDirection);
        float invLightDistance = rsqrt(lightDistanceSq);
        lightDirection *= invLightDistance;
        
        // modify 1/d^2 * R^2 to fall off at a fixed radius
        // (R/d)^2 - d/R = [(1/d^2) - (1/R^2)*(d/R)] * R^2
        float distanceFalloff = lightData.RadiusSq * (invLightDistance * invLightDistance);
        distanceFalloff = max(0, distanceFalloff - rsqrt(distanceFalloff));
        
        
        float3 L = normalize(lightDirection);

        directLighting += distanceFalloff * CalculateSpecularBrdf(
            L,
            lightData.Colour,
            N,
            V,
            NdotV,
            F0,
            albedo,
            roughness,
            metalness,
            ao);
        /*
        float attenuation = CalculateAttenuation(
                                    pointLight.ConstantAttenuation,
                                    pointLight.LinearAttenuation,
                                    pointLight.QuadraticAttenuation,
                                    distance);
        */
        
        /* OLD CODE
        // -- Calculate specular BRDF ---
    
        // Light direction;   
        // float3 L = normalize(-DirectionLightsCB.Direction.xyz);
        // float3 radiance = DirectionLightsCB.Colour.xyz;
        float3 L = normalize(-float3(0.0f, 1.0f, 0.0f));
        float3 radiance = float3(1.0f, 1.0f, 1.0f);
        // float3 L = normalize(DirectionLightsCB.PositionWS - input.positionWS);
   
        // Halfway vector - the halfway vector from the view vector to the light vector.
        float3 H = normalize(V + L);
        
        float HdotV = saturate(dot(H, V));
        float NdotL = saturate(dot(N, L));
      
        
        // Calculate Cooks tolerences terms
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(NdotV, NdotL, roughness);
        float3 F = FresnelSchlick(HdotV, F0);
        
	    // Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
	    // Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
	    // To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
        // float3 kS = F;
        //float3 kD = lerp(float3(1.0f, 1.0f, 1.0f) - kS, float3(0.0f, 0.0f, 0.0f), metalness);
        float3 kS = F;
        float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
        kD *= 1.0 - metalness;
        
        // Lambert diffuse BRDF.
	    // We don't scale by 1/PI for lighting & material units to be more convenient.
	    // See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
        float3 diffuseBRDF = kD * albedo;
        
        // Cook-Torrance specular micofacet BRDF
        // float3 specualrBRDF = (F * NDF * G) / max(Epsilon, 4.0 * NdotV * NdotL);
        float3 specualrBRDF = (F * NDF * G) / max(Epsilon, 4.0 * NdotV * NdotL);
        directLighting += (diffuseBRDF + specualrBRDF) * radiance * NdotL;
        */
    }
    // -- End Calculate specular BRDF ---
    
    // -- Fake Ambiant Lighting ---
    float3 ambientLighting = float3(0.05f, 0.05f, 0.05f) * albedo * ao;
    // -- End Fake Ambient Lighting ---
    
    return float4(directLighting + ambientLighting, 1.0f) + float4(MaterialCB.Emissive, 1.0f);
}