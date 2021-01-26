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
