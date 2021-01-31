#include "MeshPrefabs.h"
#include "Math/Common.h"
#include <stdexcept>

const D3D12_INPUT_ELEMENT_DESC VertexPositionNormalTexture::InputElements[] =
{
    { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

std::shared_ptr<Mesh> MeshPrefabs::CreateCube(
    float size,
    bool rhcoords)
{

    // A cube has six faces, each one pointing in a different direction.
    const int FaceCount = 6;

    static const Math::Vector3 faceNormals[FaceCount] =
    {
        { 0,  0,  1 },
        { 0,  0, -1 },
        { 1,  0,  0 },
        { -1,  0,  0 },
        { 0,  1,  0 },
        { 0, -1,  0 },
    };

    static const Math::XMFLOAT2 textureCoordinates[4] =
    {
        { 1, 0 },
        { 1, 1 },
        { 0, 1 },
        { 0, 0 },
    };

    VertexList vertices;
    IndexList indices;

    size /= 2;

    for (int i = 0; i < FaceCount; i++)
    {
        Math::Vector3 normal = faceNormals[i];

        // Get two vectors perpendicular both to the face normal and to each other.
        Math::Vector3 basis = (i >= 4) ? Math::Vector3(Math::kZUnitVector) : Math::Vector3(Math::kYUnitVector);

        Math::Vector3 side1 = Math::Cross(normal, basis);
        Math::Vector3 side2 = Math::Cross(normal, side1);

        // Six indices (two triangles) per face.
        size_t vbase = vertices.size();
        indices.push_back(static_cast<uint16_t>(vbase + 0));
        indices.push_back(static_cast<uint16_t>(vbase + 1));
        indices.push_back(static_cast<uint16_t>(vbase + 2));

        indices.push_back(static_cast<uint16_t>(vbase + 0));
        indices.push_back(static_cast<uint16_t>(vbase + 2));
        indices.push_back(static_cast<uint16_t>(vbase + 3));

        // Four vertices per face.
        vertices.push_back(VertexPositionNormalTexture((normal - side1 - side2) * size, normal, textureCoordinates[0]));
        vertices.push_back(VertexPositionNormalTexture((normal - side1 + side2) * size, normal, textureCoordinates[1]));
        vertices.push_back(VertexPositionNormalTexture((normal + side1 + side2) * size, normal, textureCoordinates[2]));
        vertices.push_back(VertexPositionNormalTexture((normal + side1 - side2) * size, normal, textureCoordinates[3]));
    }

    if (rhcoords)
    {
        Mesh::ReverseWinding(indices, vertices);
    }

    MeshPtr mesh = std::make_shared<Mesh>();
    mesh->m_indexData = indices;
    mesh->m_vertexData = vertices;
    mesh->material = std::make_shared<Material>();
    mesh->material->Albedo = { 1.0f, 0.0f, 0.0f, 1.0f };
    return mesh;
}

MeshPtr MeshPrefabs::CreatePlane(float width, float height, bool rhcoords)
{

    VertexList vertices =
    {
        { Math::XMFLOAT3(-0.5f * width, 0.0f,  0.5f * height), Math::XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, // 0
        { Math::XMFLOAT3(0.5f * width, 0.0f,  0.5f * height), Math::XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, // 1
        { Math::XMFLOAT3(0.5f * width, 0.0f, -0.5f * height), Math::XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, // 2
        { Math::XMFLOAT3(-0.5f * width, 0.0f, -0.5f * height), Math::XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }  // 3
    };

    IndexList indices =
    {
        0, 3, 1, 1, 3, 2
    };

    if (rhcoords)
    {
        Mesh::ReverseWinding(indices, vertices);
    }

    MeshPtr mesh = std::make_shared<Mesh>();
    mesh->m_indexData = indices;
    mesh->m_vertexData = vertices;
    mesh->material = std::make_shared<Material>();
    mesh->material->Albedo = { 1.0f, 0.0f, 0.0f, 1.0f };
    return mesh;
}

MeshPtr MeshPrefabs::CreateSphere(float diameter, size_t tesselation, bool rhcoords)
{
    if (tesselation < 3)
    {
        throw std::out_of_range("tessellation parameter out of range");
    }

    VertexList vertices;
    IndexList indices;

    float radius = diameter / 2.0f;
    size_t verticalSegments = tesselation;
    size_t horizontalSegments = tesselation * 2;


    // Create rings of vertices at progressively higher latitudes.
    for (size_t i = 0; i <= verticalSegments; i++)
    {
        float v = 1 - (float)i / verticalSegments;

        float latitude = (i * XM_PI / verticalSegments) - XM_PIDIV2;
        float dy, dxz;

        XMScalarSinCos(&dy, &dxz, latitude);

        // Create a single ring of vertices at this latitude.
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            float u = (float)j / horizontalSegments;

            float longitude = j * XM_2PI / horizontalSegments;
            float dx, dz;

            XMScalarSinCos(&dx, &dz, longitude);

            dx *= dxz;
            dz *= dxz;

            Math::Vector3 normal = Math::Vector3(dx, dy, dz);
            Math::XMFLOAT2 textureCoordinate = { u, v };

            vertices.push_back(VertexPositionNormalTexture(normal * radius, normal, textureCoordinate));
        }
    }

    // Fill the index buffer with triangles joining each pair of latitude rings.
    size_t stride = horizontalSegments + 1;
    for (size_t i = 0; i < verticalSegments; i++)
    {
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            size_t nextI = i + 1;
            size_t nextJ = (j + 1) % stride;

            indices.push_back(static_cast<uint16_t>(i * stride + j));
            indices.push_back(static_cast<uint16_t>(nextI * stride + j));
            indices.push_back(static_cast<uint16_t>(i * stride + nextJ));

            indices.push_back(static_cast<uint16_t>(i * stride + nextJ));
            indices.push_back(static_cast<uint16_t>(nextI * stride + j));
            indices.push_back(static_cast<uint16_t>(nextI * stride + nextJ));
        }
    }

    if (rhcoords)
    {
        Mesh::ReverseWinding(indices, vertices);
    }

    MeshPtr mesh = std::make_shared<Mesh>();
    mesh->m_indexData = indices;
    mesh->m_vertexData = vertices;
    mesh->material = std::make_shared<Material>();
    mesh->material->Albedo = { 1.0f, 0.0f, 0.0f, 1.0f };

    return mesh;
}
