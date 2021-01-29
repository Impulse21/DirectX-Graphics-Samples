#pragma once

#include <memory>
#include "VectorMath.h"

#include "GraphicsCore.h"

#define ALIGN(x)  __declspec(align(x))
#define ALIGN_CB  ALIGN(16)


#define BEGIN_CONSTANT_BUFFER(name) \
	ALIGN_CB struct name \
	{ \

#define END_CONSTANT_BUFFER };

// Constant Buffers
BEGIN_CONSTANT_BUFFER(Material)
Math::Vector4 Albedo = { 0.0f, 0.0f, 0.0f, 1.0f };
float Metalness = 0.0f;
float Roughness = 1.0f;
float AmbientOcclusion = 1.0f;
Math::Vector3 Emissive = { 0.0f, 0.0f, 0.0f };
END_CONSTANT_BUFFER


struct VertexPositionNormalTexture
{
	VertexPositionNormalTexture()
	{ }

	VertexPositionNormalTexture(
		Math::Vector3 const& position,
		Math::Vector3 const& normal,
		DirectX::XMFLOAT2 const& textureCoordinate)
		: position(position),
		normal(normal),
		textureCoordinate(textureCoordinate)
	{ }

	Math::Vector3 position;
	Math::Vector3 normal;
	DirectX::XMFLOAT2 textureCoordinate;

	static const int InputElementCount = 3;
	static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
};

using MaterialPtr = std::shared_ptr<Material>;

using VertexList = std::vector<VertexPositionNormalTexture>;
using IndexList = std::vector<uint16_t>;

struct Mesh
{
	std::string name;

	MaterialPtr material;

	VertexList m_vertexData;
	IndexList m_indexData;

	// Helper for flipping winding of geometric primitives for LH vs. RH coords
	static void ReverseWinding(IndexList& indices, VertexList& vertices)
	{
		assert((indices.size() % 3) == 0);
		for (auto it = indices.begin(); it != indices.end(); it += 3)
		{
			std::swap(*it, *(it + 2));
		}

		for (auto it = vertices.begin(); it != vertices.end(); ++it)
		{
			it->textureCoordinate.x = (1.f - it->textureCoordinate.x);
		}
	}
};

using MeshPtr = std::shared_ptr<Mesh>;
class MeshPrefabs
{
public:
	static std::shared_ptr<Mesh> CreateCube(
		float size,
		bool rhcoords);

	static MeshPtr CreatePlane(
		float width = 1,
		float height = 1,
		bool rhcoords = false);

	static MeshPtr CreateSphere(
		float diameter = 1,
		size_t tesselation = 16,
		bool rhcoords = false);
};

