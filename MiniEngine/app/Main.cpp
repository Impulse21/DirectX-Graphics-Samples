#include "pch.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "SystemTime.h"
#include "TextRenderer.h"
#include "GameInput.h"
#include "CommandContext.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "BufferManager.h"

#include "PostEffects.h"
#include "SSAO.h"
#include "TemporalEffects.h"
#include "FXAA.h"
#include "MotionBlur.h"

#include "Shaders/GeneratedShaders/StandardVS.h"
#include "Shaders/GeneratedShaders/StandardPS.h"

#include "Camera.h"
#include "CameraController.h"

#define TINYOBJLOADER_IMPLEMENTATION 
#include "ThridParty/TinyObj/tinyObjLoader.h"

#include <random>
#include <filesystem>

#include "MeshPrefabs.h"

#include <array>

namespace fs = std::filesystem;

using namespace GameCore;
using namespace Graphics;

using namespace Math;

BEGIN_CONSTANT_BUFFER(SceneCB)
	Matrix4 ProjectionMatrix;
	Matrix4 ViewMatrix;
	Math::XMFLOAT3 CameriaPosition;
	uint32_t NumberOmniLights = 0;
	uint32_t NumberSpotLights = 0;
END_CONSTANT_BUFFER


namespace DrawCallFlags
{
	enum
	{
		kNone = 0,
		kMultiInstance = 0x01,
	};
}

#pragma pack(1)
struct DrawCall
{
	Matrix4 Transform = {};
	uint32_t Flags = DrawCallFlags::kNone;
};

class MeshResourceLoader
{
public:
    static std::vector<MeshPtr> LoadMesh(std::string const& filename, std::vector<MaterialPtr>& materials, std::string const& baseDir = "");

private:
    static std::vector<MeshPtr> LoadFromObjFile(std::string const& filename, std::string const& baseDir, std::vector<MaterialPtr>& outMaterials);
};

struct MeshInstance
{
	MaterialPtr Material;

	StructuredBuffer VertexBuffer;
	ByteAddressBuffer IndexBuffer;

	uint32_t IndexCount = 0;
	uint32_t VertexStride;

	Matrix4 WorldTransform = Matrix4(EIdentityTag::kIdentity);
};

struct MultiMeshInstance
{
	MaterialPtr Material;

	StructuredBuffer VertexBuffer;
	ByteAddressBuffer IndexBuffer;

	uint32_t IndexCount = 0;
	uint32_t VertexStride;

	uint32_t NumInstances = 0;

	std::vector<Matrix4> Transforms;
};

namespace LightType
{
	enum : uint32_t
	{
		OmniLight,
		SpotLight,
		DirectionalLight,
	};
}

struct LightInstance
{
	DirectX::XMFLOAT3 Position;
	float radiusSq;
	DirectX::XMFLOAT3 Colour = { 1.0f, 1.0f, 1.0f };

	uint32_t Type = LightType::OmniLight;
	DirectX::XMFLOAT3 ConeDirection = {};
	DirectX::XMFLOAT3 ConeAngles = {};

};
namespace RootParameters
{
	enum
	{
		DrawCallCB,
		MaterialCB,
		TransformsSRV,
		LightDataSRV,
		SceneDataCB,
		DirectionLightCB,

		// Textures,
		NumRootParameters,
	};
}

class SanboxApp : public GameCore::IGameApp
{
public:

    SanboxApp()
    {
    }

    virtual void Startup( void ) override;
    virtual void Cleanup( void ) override;

    virtual void Update( float deltaT ) override;
    virtual void RenderScene( void ) override;

private:
	void CreatePipelineStateObjects();

	void CreateLightsScene(size_t numberOfLights, size_t NumOfSpheres = 20);

private:
    
	RootSignature m_rootSig;
	GraphicsPSO m_depthPSO;
	GraphicsPSO m_modelPSO;

    // resrouces
    std::vector<MeshPtr> m_meshResources;
	std::vector<MaterialPtr> m_materialResources;

    // Render Scene Info
    struct Scene
    {
        Math::Camera Camera;
        std::vector<MeshInstance> MeshInstances;
		std::vector<MultiMeshInstance> MultiMeshInstances;
		std::vector<LightInstance> OmniLights;

		std::vector<MeshInstance> DebugLights;
    };


	std::unique_ptr<CameraController> m_cameraController;

    Scene m_renderScene;
};

CREATE_APPLICATION( SanboxApp )


/* Distributions */
std::default_random_engine generator(1);

std::uniform_real_distribution<float> distributionXZ(-55.0, 55.0);
std::uniform_real_distribution<float> distributionY(0.0, 3.0);
std::uniform_real_distribution<float> distributionmaterial(0.3, 0.8);
std::uniform_real_distribution<float> distributionscale(1.0, 5.0);

std::uniform_real_distribution<float> distributionradius(10.0, 15.0);
std::uniform_real_distribution<float> distributionlightY(1.0, 2.0);

std::array<float, 2> intervals = { 0.0, 1.0 };
std::array<float, 2> weights{ 1500 , 1500.0 };
std::piecewise_linear_distribution<float> distributionlightcolor(intervals.begin(), intervals.end(), weights.begin());


NumVar NumberLights("Application/Lighting/Number Of Lights", 20, 1, 200);
NumVar DefaultLightHeight("Application/Lighting/Default Y", 5, -200, 200);
NumVar NumberSphere("Application/Scene/Number Of Spheres", 150, 1, 150);

BoolVar DebugDrawLights("Application/Lighting/Debug Draw Lights", true);

void SanboxApp::Startup( void )
{
	this->CreatePipelineStateObjects();

    const Math::Vector3 eye = Vector3(0.0f, 1.0f, 5.0f);
    // this->m_renderScene.Camera.SetEyeAtUp(eye, Math::Vector3(Math::kZero), Vector3(kYUnitVector));
	this->m_renderScene.Camera.SetEyeAtUp(eye, Math::Vector3(0.0f, 1.0f, 0.0f), Vector3(kYUnitVector));
    this->m_renderScene.Camera.SetZRange(1.0f, 10000.0f);
	this->m_renderScene.Camera.Update();
	this->m_cameraController.reset(new CameraController(this->m_renderScene.Camera, Vector3(kYUnitVector)));
	this->m_cameraController->SlowMovement(true);

	// Load Meshes
	// std::string baseAssetPath(fs::current_path().u8string() + "\\Assets\\Models\\CornellBox\\");

	// this->m_meshResources = MeshResourceLoader::LoadMesh(baseAssetPath + "cornell_box.obj", this->m_materialResources, baseAssetPath);

	this->CreateLightsScene(NumberLights, NumberSphere);

	MotionBlur::Enable = false;
	TemporalEffects::EnableTAA = false;
	FXAA::Enable = false;
	PostEffects::EnableHDR = false;
	PostEffects::EnableAdaptation = false;
	PostEffects::BloomEnable = false;
	SSAO::Enable = false;
}
namespace Graphics
{
	extern EnumVar DebugZoom;
}

void SanboxApp::Cleanup( void )
{
	for (int i = 0; i < this->m_renderScene.MeshInstances.size(); i++)
	{
		MeshInstance& meshInstance = this->m_renderScene.MeshInstances[i];
		meshInstance.VertexBuffer.Destroy();
		meshInstance.IndexBuffer.Destroy();
	}

	this->m_meshResources.clear();
}

void SanboxApp::Update( float deltaT )
{
    ScopedTimer _prof(L"Update State");
	if (GameInput::IsFirstPressed(GameInput::kKey_down))
	{
		Graphics::DebugZoom.Decrement();
	}	
	else if (GameInput::IsFirstPressed(GameInput::kKey_up))
	{
		Graphics::DebugZoom.Increment();
	}

	this->m_cameraController->Update(deltaT);
}

void SanboxApp::RenderScene( void )
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

    gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    gfxContext.ClearColor(g_SceneColorBuffer);
	gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
	gfxContext.ClearDepth(g_SceneDepthBuffer);
    gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV());
    gfxContext.SetViewportAndScissor(0, 0, g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());

    // Rendering something

	gfxContext.SetRootSignature(this->m_rootSig);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.SetPipelineState(this->m_modelPSO);

	{
		ScopedTimer _prof(L"Main Render", gfxContext);
		SceneCB scene = {};

		XMStoreFloat3(&scene.CameriaPosition, this->m_renderScene.Camera.GetPosition());  ;
		scene.ProjectionMatrix = this->m_renderScene.Camera.GetProjMatrix();
		scene.ViewMatrix = this->m_renderScene.Camera.GetViewMatrix();

		scene.NumberOmniLights = this->m_renderScene.OmniLights.size();
		scene.NumberSpotLights = 0;

		gfxContext.SetDynamicConstantBufferView(RootParameters::SceneDataCB, sizeof(SceneCB), &scene);

		// Set Light Buffer
		gfxContext.SetDynamicSRV(
			RootParameters::LightDataSRV,
			sizeof(LightInstance) * this->m_renderScene.OmniLights.size(),
			this->m_renderScene.OmniLights.data());

		for (auto& meshInstance : this->m_renderScene.MeshInstances)
		{
			DrawCall d = {};
			d.Transform = meshInstance.WorldTransform;
			gfxContext.SetConstant(RootParameters::DrawCallCB, d);

			auto m = meshInstance.Material;
			gfxContext.SetDynamicConstantBufferView(RootParameters::MaterialCB, sizeof(*m), m.get());
			gfxContext.SetIndexBuffer(meshInstance.IndexBuffer.IndexBufferView());
			gfxContext.SetVertexBuffer(0, meshInstance.VertexBuffer.VertexBufferView());

			gfxContext.DrawIndexed(meshInstance.IndexCount, 0, 0);
		}

		for (auto& multiMeshInstance: this->m_renderScene.MultiMeshInstances)
		{
			DrawCall d = {};
			d.Flags = DrawCallFlags::kMultiInstance;
			d.Transform = Matrix4(kIdentity);
			
			gfxContext.SetConstant(RootParameters::DrawCallCB, d);

			gfxContext.SetDynamicSRV(
				RootParameters::TransformsSRV,
				multiMeshInstance.Transforms.size() * sizeof(Matrix4),
				multiMeshInstance.Transforms.data());

			auto m = multiMeshInstance.Material;
			gfxContext.SetDynamicConstantBufferView(RootParameters::MaterialCB, sizeof(*m), m.get());
			gfxContext.SetIndexBuffer(multiMeshInstance.IndexBuffer.IndexBufferView());
			gfxContext.SetVertexBuffer(0, multiMeshInstance.VertexBuffer.VertexBufferView());
			gfxContext.DrawIndexedInstanced(multiMeshInstance.IndexCount, multiMeshInstance.NumInstances, 0, 0, 0);
		}
	}

    gfxContext.Finish();
}

void SanboxApp::CreatePipelineStateObjects()
{
	SamplerDesc defaultSamplerDesc;
	defaultSamplerDesc.MaxAnisotropy = 8;

	this->m_rootSig.Reset(RootParameters::NumRootParameters, 1);
	this->m_rootSig.InitStaticSampler(0, defaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	this->m_rootSig[RootParameters::DrawCallCB].InitAsConstants(0, sizeof(DrawCall) / sizeof(uint32_t));
	this->m_rootSig[RootParameters::MaterialCB].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_PIXEL);
	this->m_rootSig[RootParameters::TransformsSRV].InitAsBufferSRV(0, D3D12_SHADER_VISIBILITY_VERTEX);
	this->m_rootSig[RootParameters::LightDataSRV].InitAsBufferSRV(1, D3D12_SHADER_VISIBILITY_PIXEL);
	this->m_rootSig[RootParameters::SceneDataCB].InitAsConstantBuffer(2);
	this->m_rootSig[RootParameters::DirectionLightCB].InitAsConstantBuffer(3, D3D12_SHADER_VISIBILITY_PIXEL);

	// this->m_rootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 6, D3D12_SHADER_VISIBILITY_PIXEL);
	// this->m_rootSig[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 64, 6, D3D12_SHADER_VISIBILITY_PIXEL);
	this->m_rootSig.Finalize(L"Standard", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	DXGI_FORMAT ColorFormat = g_SceneColorBuffer.GetFormat();
	DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();

	// Depth-only (2x rate)
	this->m_depthPSO.SetRootSignature(this->m_rootSig);
	this->m_depthPSO.SetRasterizerState(RasterizerDefault);
	this->m_depthPSO.SetBlendState(BlendNoColorWrite);
	this->m_depthPSO.SetDepthStencilState(DepthStateReadWrite);
	this->m_depthPSO.SetInputLayout(VertexPositionNormalTexture::InputElementCount, VertexPositionNormalTexture::InputElements);
	this->m_depthPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	this->m_depthPSO.SetRenderTargetFormats(0, nullptr, DepthFormat);
	this->m_depthPSO.SetVertexShader(g_pStandardVS, sizeof(g_pStandardVS));
	this->m_depthPSO.Finalize();

	// Full color pass
	this->m_modelPSO = this->m_depthPSO;
	this->m_modelPSO.SetBlendState(BlendDisable);
	this->m_modelPSO.SetDepthStencilState(DepthStateReadWrite);
	this->m_modelPSO.SetRenderTargetFormats(1, &ColorFormat, DepthFormat);
	this->m_modelPSO.SetVertexShader(g_pStandardVS, sizeof(g_pStandardVS));
	this->m_modelPSO.SetPixelShader(g_pStandardPS, sizeof(g_pStandardPS));
	this->m_modelPSO.Finalize();
}

void SanboxApp::CreateLightsScene(size_t numberOfLights, size_t NumOfSpheres)
{
	{
		auto floorMesh = MeshPrefabs::CreatePlane(120.0f, 120.0f, true);
		floorMesh->material->Albedo = Vector4(0.3f, 0.3f, 0.3f, 1.0f);

		MeshInstance floorInstance = {};
		floorInstance.Material = floorMesh->material;
		floorInstance.VertexBuffer.Create(L"VertexBuffer", floorMesh->m_vertexData.size(), sizeof(VertexPositionNormalTexture), floorMesh->m_vertexData.data());
		floorInstance.IndexBuffer.Create(L"IndexBuffer", floorMesh->m_indexData.size(), sizeof(uint16_t), floorMesh->m_indexData.data());
		floorInstance.IndexCount = floorMesh->m_indexData.size();
		floorInstance.WorldTransform = Matrix4(kIdentity);

		this->m_meshResources.push_back(floorMesh);
		this->m_renderScene.MeshInstances.emplace_back(floorInstance);
	}

		auto ballMesh = MeshPrefabs::CreateSphere(2.0f, 16, true);
	{
		this->m_meshResources.push_back(ballMesh);

			MultiMeshInstance ballInstances = {};
			ballInstances.Material = ballMesh->material;
			ballInstances.Material->Albedo = { 0.5f, 0.5f, 0.5f, 1.0f };
			ballInstances.VertexBuffer.Create(L"VertexBuffer", ballMesh->m_vertexData.size(), sizeof(VertexPositionNormalTexture), ballMesh->m_vertexData.data());
			ballInstances.IndexBuffer.Create(L"IndexBuffer", ballMesh->m_indexData.size(), sizeof(uint16_t), ballMesh->m_indexData.data());
			ballInstances.IndexCount = ballMesh->m_indexData.size();

			ballInstances.NumInstances = NumOfSpheres;
			ballInstances.Transforms.resize(NumOfSpheres);
			for (int i = 0; i < NumOfSpheres; i++)
			{
				const float s = distributionscale(generator);
				ballInstances.Transforms[i] = Matrix4(AffineTransform(Matrix3::MakeScale(s, s, s), Vector3(distributionXZ(generator), distributionY(generator), distributionXZ(generator))));
			}

			this->m_renderScene.MultiMeshInstances.emplace_back(ballInstances);
	}

	{
		this->m_renderScene.OmniLights.resize(numberOfLights);

		for (int i = 0; i < numberOfLights; i++)
		{
			LightInstance& light = this->m_renderScene.OmniLights[i];
			light.Colour = { distributionlightcolor(generator), distributionlightcolor(generator), distributionlightcolor(generator) };
			light.Position = { distributionXZ(generator), DefaultLightHeight, distributionXZ(generator) };
			light.Type = LightType::OmniLight;
			const float radius = distributionradius(generator);
			light.radiusSq = radius * radius;

			if (DebugDrawLights)
			{
				auto material = std::make_shared<Material>();
				material->Albedo = Vector4(0.0f);
				material->Roughness = 0.0f;
				material->Metalness = 0.0f;
				material->AmbientOcclusion = 0.0f;
				material->Emissive = light.Colour;

				MeshInstance debugLightMesh = {};
				debugLightMesh.Material = material;
				debugLightMesh.VertexBuffer.Create(L"VertexBuffer", ballMesh->m_vertexData.size(), sizeof(VertexPositionNormalTexture), ballMesh->m_vertexData.data());
				debugLightMesh.IndexBuffer.Create(L"IndexBuffer", ballMesh->m_indexData.size(), sizeof(uint16_t), ballMesh->m_indexData.data());
				debugLightMesh.IndexCount = ballMesh->m_indexData.size();
				debugLightMesh.WorldTransform = Matrix4(AffineTransform(Matrix3::MakeScale(0.3f, 0.3f, 0.3f), light.Position));

				this->m_renderScene.MeshInstances.push_back(debugLightMesh);
			}
		}
	}
}

#pragma region Mesh Loader functions
std::vector<MeshPtr> MeshResourceLoader::LoadMesh(std::string const& filename, std::vector<MaterialPtr>& materials, std::string const& baseDir)
{
    fs::path filePath(filename);
    if (!fs::exists(filePath))
    {
        Utility::Printf("Unalbe to load Mesh\n");
        throw std::exception("File not found.");
    }

    if (filePath.extension() == ".obj")
    {
        return LoadFromObjFile(filename, baseDir, materials);
    }

    return std::vector<MeshPtr>();
}

std::vector<MeshPtr> MeshResourceLoader::LoadFromObjFile(std::string const& filename, std::string const& baseDir, std::vector<MaterialPtr>& outMaterials)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warning, error;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, filename.c_str(), baseDir.c_str()))
	{
		if (!error.empty())
		{
			Utility::Printf("Failed to load obj: %s\n", error.c_str());
		}

		return std::vector<MeshPtr>();
	}

	if (!warning.empty())
	{
		Utility::Printf("Warnings thrown when loading obj: %s\n", warning.c_str());
	}

	outMaterials.resize(materials.size());

	for (auto iMtl = 0; iMtl < materials.size(); iMtl++)
	{
		auto& objMtl = materials[iMtl];
		auto& mtl = std::make_shared<Material>();

		Utility::Printf("Loading Material %s\n", objMtl.name.c_str());
		mtl->Albedo = { objMtl.diffuse[0], objMtl.diffuse[1], objMtl.diffuse[2], 1.0f };
		mtl->Roughness = objMtl.roughness;
		mtl->Metalness = objMtl.metallic;
		mtl->Emissive = { objMtl.emission[0], objMtl.emission[1], objMtl.emission[2] };
		outMaterials[iMtl] = mtl;
	}

	std::vector<MeshPtr> meshes(shapes.size());
	for (int i = 0; i < shapes.size(); i++)
	{
		const auto& shape = shapes[i];

		VertexList vertices;
		IndexList indices;

		Utility::Printf("Loading Mesh : %s\n", shape.name.c_str());

		for (const auto& index : shape.mesh.indices)
		{
			VertexPositionNormalTexture vertex;

			vertex.position =
			{
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2],
			};

			vertex.normal =
			{
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2],
			};

			vertex.textureCoordinate =
			{
				attrib.texcoords[2 * index.texcoord_index + 0],
				attrib.texcoords[2 * index.texcoord_index + 1],
			};
			vertices.emplace_back(vertex);
			indices.push_back(indices.size());

			// TODO: Fiter out redundent Vertices
		}
		/* TODO:
		auto meshNode = GetScene().CreateNode();
		meshNode->Name = shape.name;
		meshNode->Mesh = std::make_shared<Mesh>(commandList, vertices, indices, true);
		meshNode->PbrMaterial = pbrMaterials[shape.mesh.material_ids[0]];
		node.AddChild(meshNode);
		*/

		// Mesh::ReverseWinding(indices, vertices);
		meshes[i] = std::make_shared<Mesh>();
		meshes[i]->m_indexData = indices;
		meshes[i]->m_vertexData = vertices;
		meshes[i]->material = outMaterials[shape.mesh.material_ids[0]];
	}

    return meshes;
}

#pragma endregion