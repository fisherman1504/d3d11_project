#include "stdafx.h"
#include "Mesh.h"

/*
 * Mesh::Mesh
 */
Mesh::Mesh(
        std::vector<Vertex> vertices,
        std::vector<unsigned int> indices,
        std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout,
		std::vector<Texture> textures,
        Material matDefinition,
        std::wstring vertexShaderName,
        std::wstring pixelShaderName,
        wrl::ComPtr<ID3D11Device> d3dDevice,
        wrl::ComPtr<ID3D11DeviceContext> d3dContext) {
    // Store inputs.
    m_vertices = vertices;
    m_indices = indices;
    m_textures = textures;
    m_d3dDevice = d3dDevice;
    m_d3dContext = d3dContext;
    m_matDefinition = matDefinition;
    m_vertexLayout = vertexLayout;

    // Instanced rendering information.
    m_instanceStride = 0;
    m_instanceCount = 0;
    m_instanceOffset = 0;
    m_usesInstancing = false;

    // In case not layout is provided, use standard layout.
    if (m_vertexLayout.size() == 0) {
        m_vertexLayout = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // Second entry (0) defines semantic index --> TEXCOORD0

            // For normal mapping.
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // D3D11_APPEND_ALIGNED_ELEMENT for automatic packing
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // Second entry (0) defines semantic index --> TEXCOORD0
            {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 } // Second entry (0) defines semantic index --> TEXCOORD0
        };
    }

    // Setup shaders for rendering the mesh.
    setupShaders(vertexShaderName, pixelShaderName);

    // Setup buffers on the GPU.
    setupMesh();
}


/*
 * Mesh::Draw
 */
void Mesh::Draw(bool depthPass) {
    // Setup instruction assembly.
    m_d3dContext->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_d3dContext->IASetInputLayout(m_vertexDataLayout.Get());

    // Set texture sampler.
    m_d3dContext->PSSetSamplers(0, 1, m_sampleState.GetAddressOf());

    // Set index buffer.
    m_d3dContext->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    // Set vertex (+instance) buffer.
    if (m_usesInstancing) {
        std::array<unsigned int, 2> strides = { m_vertexStride, m_instanceStride };
        std::array<unsigned int, 2> offsets = { m_vertexOffset, m_instanceOffset };
        std::array<ID3D11Buffer*, 2> bufferPointers = { m_vertexBuffer.Get(), m_instanceBuffer.Get() };
        m_d3dContext->IASetVertexBuffers(0, 2, bufferPointers.data(),
            strides.data(), offsets.data());
    } else {
        m_d3dContext->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(),
            &m_vertexStride, &m_vertexOffset);
    }

    // Choose required shaders.
    if (depthPass) {
        // Setup vertex shader.
        m_d3dContext->VSSetShader(m_shadowVS.Get(), 0, 0);

        // Setup Pixel shader. To avoid DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET.
        m_d3dContext->PSSetShader(0, 0, 0);
    } else {
        // Setup vertex shader.
        m_d3dContext->VSSetShader(m_vertexShader.Get(), 0, 0);

        // Setup Pixel shader.
        m_d3dContext->PSSetShader(m_pixelShader.Get(), 0, 0);
    }

    // Setup constant buffer that contains information from current mesh:
    // matShininess, matOpticalDensity,...
    m_d3dContext->PSSetConstantBuffers(0, 1, m_constBufferPS.GetAddressOf());

    // Bind textures. This will only be performed for models that were loaded from
    // disk --> ModelClass::BaseType::LOADED
    for (unsigned int texIdx = 0; texIdx < m_textures.size(); texIdx++) {
        std::string name = m_textures[texIdx].type;
        if (name == "texture_ambient") {
            m_d3dContext->PSSetShaderResources(0, 1, m_textures[texIdx].srv.GetAddressOf());
        } else if (name == "texture_diffuse") {
            m_d3dContext->PSSetShaderResources(1, 1, m_textures[texIdx].srv.GetAddressOf());
        } else if (name == "texture_specular") {
            m_d3dContext->PSSetShaderResources(2, 1, m_textures[texIdx].srv.GetAddressOf());
        } else if (name == "texture_normal") {
            m_d3dContext->PSSetShaderResources(3, 1, m_textures[texIdx].srv.GetAddressOf());
        } else if (name == "texture_bump") {
            m_d3dContext->PSSetShaderResources(4, 1, m_textures[texIdx].srv.GetAddressOf());
        } else if (name == "texture_dissolve") {
            m_d3dContext->PSSetShaderResources(5, 1, m_textures[texIdx].srv.GetAddressOf());
        } else if (name == "texture_emissive") {
            m_d3dContext->PSSetShaderResources(6, 1, m_textures[texIdx].srv.GetAddressOf());
        }
    }

    // Make the draw call.
    if (m_usesInstancing) {
        // Draw index+instanced primitives. Uses currently bound vertex, index and
        // instance buffer.
        m_d3dContext->DrawIndexedInstanced(m_indices.size(),m_instanceCount, 0, 0, 0);
    } else {
        // Draw indexed, non-instanced primitives. Uses currently bound vertex and index
        // buffer.
        m_d3dContext->DrawIndexed(m_indices.size(), 0, 0);
    }

    // Cleanup.
    ID3D11ShaderResourceView* nullSRV[10] = { nullptr };
    m_d3dContext->VSSetShaderResources(0, 10, nullSRV);
    m_d3dContext->PSSetShaderResources(0, 10, nullSRV);
}


/*
 * Mesh::setupMesh
 */
void Mesh::setupMesh() {
    // Set vertex buffer information.
    m_vertexStride = sizeof(Vertex);
    m_vertexOffset = 0; // TODO: Maybe change in the future.

    // Create vertex input layout.
    HRESULT hr = m_d3dDevice->CreateInputLayout(m_vertexLayout.data(),
        m_vertexLayout.size(), m_vertexShaderByteCode->GetBufferPointer(),
        m_vertexShaderByteCode->GetBufferSize(), m_vertexDataLayout.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Fill in a buffer description.
    D3D11_BUFFER_DESC vertexBufferDesc;
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(Vertex) * m_vertices.size();
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;
    vertexBufferDesc.MiscFlags = 0;

    // Fill in the subresource data.
    D3D11_SUBRESOURCE_DATA vertexInitData;
    vertexInitData.pSysMem = m_vertices.data();
    vertexInitData.SysMemPitch = 0;
    vertexInitData.SysMemSlicePitch = 0;

    // Create the vertex buffer.
    hr = m_d3dDevice->CreateBuffer(&vertexBufferDesc, &vertexInitData,
        m_vertexBuffer.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Fill in a buffer description.
    D3D11_BUFFER_DESC indexBufferDesc;
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = sizeof(unsigned int) * m_indices.size();
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;

    // Define the resource data.
    D3D11_SUBRESOURCE_DATA indexInitData;
    indexInitData.pSysMem = m_indices.data();
    indexInitData.SysMemPitch = 0;
    indexInitData.SysMemSlicePitch = 0;

    // Create the index buffer with the device.
    hr = m_d3dDevice->CreateBuffer(&indexBufferDesc, &indexInitData,
        m_indexBuffer.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Init texture sampler settings.
    D3D11_SAMPLER_DESC samplerDesc;
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;  // GL_REPEAT
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;  // GL_REPEAT
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;  // GL_REPEAT
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.BorderColor[0] = 0;
    samplerDesc.BorderColor[1] = 0;
    samplerDesc.BorderColor[2] = 0;
    samplerDesc.BorderColor[3] = 0;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX; // Use all mip maps.
    hr = m_d3dDevice->CreateSamplerState(
        &samplerDesc, m_sampleState.GetAddressOf());

    // Create constant buffer for pixel shader. Does not get updated every frame.
    Material constBufferPSData = {};
    constBufferPSData.matAmbientColor = m_matDefinition.matAmbientColor;
    constBufferPSData.matDiffuseColor = m_matDefinition.matDiffuseColor;
    constBufferPSData.matSpecularColor = m_matDefinition.matSpecularColor;
    constBufferPSData.matShininess = m_matDefinition.matShininess;
    constBufferPSData.matOpticalDensity = m_matDefinition.matOpticalDensity;
    constBufferPSData.matDissolveFactor = m_matDefinition.matDissolveFactor;
    constBufferPSData.matColorViaTex = m_matDefinition.matColorViaTex;

    // Fill in a buffer description.
    D3D11_BUFFER_DESC cbPSDesc;
    cbPSDesc.ByteWidth = sizeof(Material);
    cbPSDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbPSDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbPSDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbPSDesc.MiscFlags = 0;
    cbPSDesc.StructureByteStride = 0;

    // Fill in the subresource data.
    D3D11_SUBRESOURCE_DATA constInitDataPS;
    constInitDataPS.pSysMem = &constBufferPSData;
    constInitDataPS.SysMemPitch = 0;
    constInitDataPS.SysMemSlicePitch = 0;

    // Create the buffer.
    hr = m_d3dDevice->CreateBuffer(&cbPSDesc, &constInitDataPS,
        m_constBufferPS.GetAddressOf());
    assert(SUCCEEDED(hr));
}


/*
 * Mesh::setupShaders
 */
void Mesh::setupShaders(std::wstring vertexShaderName,
        std::wstring pixelShaderName){
    // Init shaders.
    Helper::CreateVertexShader(vertexShaderName.c_str(),
        m_vertexShaderByteCode, m_vertexShader, m_d3dDevice);
    Helper::CreatePixelShader(pixelShaderName.c_str(),
        m_pixelShaderByteCode, m_pixelShader, m_d3dDevice);

    // Shaders for light view pass (shadow mapping).
    Helper::CreateVertexShader(L"\\src\\shader\\Shadow_vs.hlsl",
        m_shadowVSByteCode, m_shadowVS, m_d3dDevice);
    Helper::CreatePixelShader(L"\\src\\shader\\Shadow_ps.hlsl",
        m_shadowPSByteCode, m_shadowPS, m_d3dDevice);

    // Shaders for instanced rendering. TODO: Make customizable.
    //Helper::CreateVertexShader(L"\\src\\shader\\BasicInstanced_vs.hlsl",
    //    m_vertexShaderInstancedByteCode, m_vertexShaderInstanced, m_d3dDevice);
    //Helper::CreatePixelShader(L"\\src\\shader\\BasicInstanced_ps.hlsl",
    //    m_pixelShaderInstancedByteCode, m_pixelShaderInstanced, m_d3dDevice);
}


/*
 * Mesh::SetupInstancing
 */
void Mesh::SetupInstancing(
        wrl::ComPtr<ID3D11Buffer> instanceBuffer,
        unsigned int instanceCount,
        unsigned int instanceStride) {
    // Store information.
    m_instanceCount = instanceCount;
    m_instanceStride = instanceStride;
    m_instanceBuffer = instanceBuffer;
    m_usesInstancing = true;
}
