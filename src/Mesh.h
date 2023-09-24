#pragma once
#include "Helper.h"
#include "DirectXMesh.h"

/// <summary>
/// Describes contents of a vertex.
/// </summary>
/// <remarks>
/// Currently used for all possible models. Sometimes space is wasted. TODO: Improve.
/// </remarks>
__declspec(align(16)) struct Vertex {
    sm::Vector3 Position;
    sm::Vector2 TexCoords;

    // Normal Mapping.
    sm::Vector3 Normal;     // Object/model space.
    sm::Vector3 Tangent;    // Object/model space.
    sm::Vector3 Bitangent;  // Object/model space.

    Vertex(sm::Vector3 pos, sm::Vector2 texCoords, sm::Vector3 normal,
            sm::Vector3 tangent, sm::Vector3 bitangent) {
        Position = pos;
        TexCoords = texCoords;

        Normal = normal;
        Tangent = tangent;
        Bitangent = bitangent;
    }

    // For supporting cube, sphere, ...
    Vertex(sm::Vector3 pos, sm::Vector3 normal, sm::Vector2 texCoords) {
        Position = pos;
        TexCoords = texCoords;

        Normal = normal;
        Tangent = sm::Vector3(0.0,0.0,0.0);
        Bitangent = sm::Vector3(0.0, 0.0, 0.0);
    }

    // For custom mesh.
    Vertex(sm::Vector3 pos, sm::Vector2 texCoords) {
        Position = pos;
        TexCoords = texCoords;

        Normal = sm::Vector3(0.0, 0.0, 0.0);
        Tangent = sm::Vector3(0.0, 0.0, 0.0);
        Bitangent = sm::Vector3(0.0, 0.0, 0.0);
    }

    // Default.
    Vertex() {
        Position = sm::Vector3(0.0,0.0,0.0);
        TexCoords = sm::Vector2(0.0, 0.0);

        Normal = sm::Vector3(0.0, 0.0, 0.0);
        Tangent = sm::Vector3(0.0, 0.0, 0.0);
        Bitangent = sm::Vector3(0.0, 0.0, 0.0);
    }
};

/// <summary>
/// Important information about a texture. Assimp will use this structure.
/// </summary>
struct Texture {
    wrl::ComPtr<ID3D11ShaderResourceView> srv;
    std::string type;
    std::string path;
};

/// <summary>
/// Important information about a texture. Later used for PBR.
/// </summary>
struct Material {
    sm::Vector3 matAmbientColor;
    float matPadding0;

    sm::Vector3 matDiffuseColor;
    float matPadding1;

    sm::Vector3 matSpecularColor;
    float matPadding2;

    // PBR information.
    float matShininess;
    float matOpticalDensity;
    float matDissolveFactor;
    bool matColorViaTex;
    bool matPadding3;
    bool matPadding4;
    bool matPadding5;
};

/// <summary>
/// Class that contains all resources of a 3D model.
/// </summary>
/// <remarks>
/// Taken from https://learnopengl.com/Model-Loading/Mesh and adapted to D3D11.
/// </remarks>
class Mesh {
public:
    /// <summary>
    /// Constructor.
    /// </summary>
    /// <param name="vertices">Vertex data of the mesh.</param>
    /// <param name="indices">Index data of the mesh.</param>
    /// <param name="vertexLayout">Vertex layout of the mesh.</param>
    /// <param name="textures">Textures of the mesh (diffuse, normals, ...)</param>
    /// <param name="matDefinition">PBR information.</param>
    /// <param name="vertexShaderName">Path to the vertex shader.</param>
    /// <param name="pixelShaderName">Path to the pixel shader.</param>
    /// <param name="d3dDevice">D3D11 device.</param>
    /// <param name="d3dContext">D3D11 context.</param>
    Mesh(
        std::vector<Vertex> vertices,
        std::vector<unsigned int> indices,
        std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout,
        std::vector<Texture> textures,
        Material matDefinition,
        std::wstring vertexShaderName,
        std::wstring pixelShaderName,
        wrl::ComPtr<ID3D11Device> d3dDevice,
        wrl::ComPtr<ID3D11DeviceContext> d3dContext);

    /// <summary>
    /// Render the mesh.
    /// </summary>
    /// <param name="depthPass">Indicates if the shadow shaders should be used or
    /// not.</param>
    void Draw(bool depthPass);

    /// <summary>
    /// Init instance buffer for instanced rendering of this mesh.
    /// </summary>
    void SetupInstancing(
        wrl::ComPtr<ID3D11Buffer> instanceBuffer,
        unsigned int instanceCount,
        unsigned int instanceStride);

private:
    /// <summary>
    /// Creates buffers and samplers for the mesh.
    /// </summary>
    void setupMesh();

    /// <summary>
    /// Init shaders for normal rendering and light pass.
    /// </summary>
    /// <param name="vertexShaderName">Path to vertex shader.</param>
    /// <param name="pixelShaderName">Path to pixel shader.</param>
    void setupShaders(std::wstring vertexShaderName, std::wstring pixelShaderName);

    // Mesh data.
    std::vector<Vertex>       m_vertices;
    std::vector<unsigned int> m_indices;
    std::vector<Texture>      m_textures;

    // Vertex and Index Buffer on GPU.
    wrl::ComPtr<ID3D11Buffer> m_vertexBuffer;
    wrl::ComPtr<ID3D11Buffer> m_indexBuffer;
    std::vector<D3D11_INPUT_ELEMENT_DESC> m_vertexLayout;
    wrl::ComPtr<ID3D11SamplerState> m_sampleState;

    // Material information (->PBR): matShininess, matOpticalDensity,...
    Material m_matDefinition;
    wrl::ComPtr<ID3D11Buffer> m_constBufferPS;

    // Shaders.
    wrl::ComPtr<ID3DBlob> m_vertexShaderByteCode;
    wrl::ComPtr<ID3DBlob> m_pixelShaderByteCode;
    wrl::ComPtr<ID3D11VertexShader> m_vertexShader;
    wrl::ComPtr<ID3D11PixelShader> m_pixelShader;
    wrl::ComPtr <ID3D11InputLayout> m_vertexDataLayout;

    // Shaders for directional light view pass (shadow mapping).
    wrl::ComPtr<ID3DBlob> m_shadowVSByteCode;
    wrl::ComPtr<ID3DBlob> m_shadowPSByteCode;
    wrl::ComPtr<ID3D11VertexShader> m_shadowVS;
    wrl::ComPtr<ID3D11PixelShader> m_shadowPS;

    // Direct3D stuff.
    wrl::ComPtr<ID3D11Device> m_d3dDevice;
    wrl::ComPtr<ID3D11DeviceContext> m_d3dContext;

    // Other mesh information.
    unsigned int  m_vertexStride;
    unsigned int m_vertexOffset;

    // Instanced rendering information (obtained from ModelClass).
    bool m_usesInstancing;
    unsigned int m_instanceCount;
    unsigned int m_instanceStride;
    unsigned int m_instanceOffset;
    wrl::ComPtr<ID3D11Buffer> m_instanceBuffer;
};