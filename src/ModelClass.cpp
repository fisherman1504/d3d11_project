#include "stdafx.h"
#include "ModelClass.h"

/*
 * ModelClass::ModelClass
 */
ModelClass::ModelClass(
        std::string directory,
        std::string name,
        unsigned int modelExtraFlags,
        wrl::ComPtr<ID3D11Device> d3dDevice,
        wrl::ComPtr<ID3D11DeviceContext> d3dContext,
        const sm::Matrix* viewMat,
        float initScale,
        sm::Vector3 initPosition,
        sm::Vector4 initRotation,
        unsigned int fileFormat,
        std::wstring vertexShaderName,
        std::wstring pixelShaderName) {
    // Store information.
    m_d3dDevice = d3dDevice;
    m_d3dContext = d3dContext;
    m_viewMat = viewMat;
    m_directory = directory;
    m_name = name;
    m_modelExtraFlags = modelExtraFlags;
    m_fileFormat = fileFormat;
    m_vertexShaderName = vertexShaderName;
    m_pixelShaderName = pixelShaderName;
    m_baseType = ModelClass::BaseType::LOADED;

    // Setup model path.
    switch (fileFormat) {
    case 0:
        m_fullModelPath = m_directory + name + ".obj";
        break;
    case 1:
        m_fullModelPath = m_directory + name + ".dae";
        break;
    default:
        m_fullModelPath = m_directory + name + ".obj";
    }

    // Information about the model.
    m_state = ModelState(initScale, initPosition, sm::Quaternion(initRotation));
    m_modelMat = sm::Matrix::CreateTranslation(m_state.position)
        * sm::Matrix::CreateFromQuaternion(m_state.orientation)
        * sm::Matrix::CreateScale(m_state.scale);

    // Transpose at the end is missing on purpose, since the matrix will get
    // transposed afterwards one more time (for GPU upload).
    m_normalMat = ((*m_viewMat) * m_modelMat).Invert().Transpose();

    // Load data and create necessary vertex/index buffers, textures, ...
    loadModel();

    // Create necessary constant buffers that are needed.
    initBuffers();
}


/*
 * ModelClass::ModelClass
 */
ModelClass::ModelClass(ModelClass::BaseType modelType,
        sm::Vector3 color,
        wrl::ComPtr<ID3D11Device> d3dDevice,
        wrl::ComPtr<ID3D11DeviceContext> d3dContext,
        const sm::Matrix* viewMat,
        float initScale,
        sm::Vector3 initPosition,
        sm::Vector4 initRotation,
        std::wstring vertexShaderName,
        std::wstring pixelShaderName) {
    // Store information.
    m_d3dDevice = d3dDevice;
    m_d3dContext = d3dContext;
    m_viewMat = viewMat;
    m_vertexShaderName = vertexShaderName;
    m_pixelShaderName = pixelShaderName;
    m_baseType = modelType;

    // Information about the model.
    m_state = ModelState(initScale, initPosition, sm::Quaternion(initRotation));
    m_modelMat = sm::Matrix::CreateTranslation(m_state.position)
        * sm::Matrix::CreateFromQuaternion(m_state.orientation)
        * sm::Matrix::CreateScale(m_state.scale);

    // Transpose at the end is missing on purpose, since the matrix will get
    // transposed afterwards one more time (for GPU upload).
    m_normalMat = ((*m_viewMat) * m_modelMat).Invert().Transpose();

    // Load data and create necessary vertex/index buffers, textures, ...
    if (m_baseType == ModelClass::BaseType::CUBE) {
        createCubeMesh(color, false);
    } else if (m_baseType == ModelClass::BaseType::SPHERE) {
        createSphereMesh(color, false);
    } else if (m_baseType == ModelClass::BaseType::TORUS) {
        createTorusMesh(color, false);
    } else {
       throw std::invalid_argument("Type not implemented.");
    }

    // Create necessary constant buffers that are needed.
    initBuffers();
}


/*
 * ModelClass::ModelClass
 */
ModelClass::ModelClass(
        BaseType modelType,
        std::vector<sm::Vector3> positions,
        std::vector<sm::Vector3> colors,
        std::vector<sm::Vector3> scales,
        std::vector<sm::Vector4> rotations,
        wrl::ComPtr<ID3D11Device> d3dDevice,
        wrl::ComPtr<ID3D11DeviceContext> d3dContext,
        const sm::Matrix* viewMat,
        std::wstring vertexShaderName,
        std::wstring pixelShaderName) {
    // Store information.
    m_d3dDevice = d3dDevice;
    m_d3dContext = d3dContext;
    m_viewMat = viewMat;
    m_vertexShaderName = vertexShaderName;
    m_pixelShaderName = pixelShaderName;
    m_baseType = modelType;

    // Information about the model. TODO: Fix model state creation.
    m_state = ModelState(1.0, sm::Vector3(0.0,0.0,0.0),
        sm::Quaternion(sm::Vector4(0.0,0.0,0.0,1.0)));

    // TODO: Fix. Not used for this case.
    m_modelMat = sm::Matrix::CreateTranslation(m_state.position)
        * sm::Matrix::CreateFromQuaternion(m_state.orientation)
        * sm::Matrix::CreateScale(m_state.scale);

    // TODO: Fix. Not used for this case.
    m_normalMat = ((*m_viewMat) * m_modelMat).Invert().Transpose();

    // Create instance buffer.
    m_instanceCount = positions.size();
    m_instanceStride = sizeof(InstanceType);

    // Color is ignored here. Since we use the color from the instance buffer.
    if (m_baseType == ModelClass::BaseType::CUBE) {
        createCubeMesh(sm::Vector3(0.0,0.0,0.0), m_instanceCount);
    } else if (m_baseType == ModelClass::BaseType::SPHERE) {
        createSphereMesh(sm::Vector3(0.0, 0.0, 0.0), m_instanceCount);
    } else if (m_baseType == ModelClass::BaseType::TORUS) {
        createTorusMesh(sm::Vector3(0.0, 0.0, 0.0), m_instanceCount);
    } else {
        throw std::invalid_argument("Type not implemented.");
    }

    std::vector<InstanceType> iBufferData;
    for (unsigned int i = 0; i < m_instanceCount; i++) {
        InstanceType element;
        element.iPos = positions[i];
        element.iScale = scales[i];
        element.iColor = colors[i];
        iBufferData.push_back(element);
    }

    // Fill in a buffer description.
    D3D11_BUFFER_DESC vertexBufferDesc;
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(InstanceType) * m_instanceCount;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;
    vertexBufferDesc.MiscFlags = 0;

    // Fill in the subresource data.
    D3D11_SUBRESOURCE_DATA vertexInitData;
    vertexInitData.pSysMem = iBufferData.data();
    vertexInitData.SysMemPitch = 0;
    vertexInitData.SysMemSlicePitch = 0;

    // Create the vertex buffer.
    HRESULT hr = m_d3dDevice->CreateBuffer(&vertexBufferDesc, &vertexInitData,
        m_instanceBuffer.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Pass information to the only Mesh object of this ModelClass instance.
    m_meshes[0].SetupInstancing(m_instanceBuffer, m_instanceCount, m_instanceStride);

    // Create other necessary buffers that are needed.
    initBuffers();
}


/*
 * ModelClass::ModelClass
 */
ModelClass::ModelClass(
        std::vector<Vertex> vertices,
        std::vector<unsigned int> indices,
        std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout,
        Material matDefinition,
        wrl::ComPtr<ID3D11Device> d3dDevice,
        wrl::ComPtr<ID3D11DeviceContext> d3dContext,
        const sm::Matrix* viewMat,
        float initScale,
        sm::Vector3 initPosition,
        sm::Vector4 initRotation,
        std::wstring vertexShaderName,
        std::wstring pixelShaderName) {
    // Store information.
    m_d3dDevice = d3dDevice;
    m_d3dContext = d3dContext;
    m_viewMat = viewMat;
    m_vertexShaderName = vertexShaderName;
    m_pixelShaderName = pixelShaderName;
    m_baseType = ModelClass::BaseType::CUSTOM;

    // Information about the model.
    m_state = ModelState(initScale, initPosition, sm::Quaternion(initRotation));
    m_modelMat = sm::Matrix::CreateTranslation(m_state.position)
        * sm::Matrix::CreateFromQuaternion(m_state.orientation)
        * sm::Matrix::CreateScale(m_state.scale);

    // Transpose at the end is missing on purpose, since the matrix will get
    // transposed afterwards one more time (for GPU upload).
    m_normalMat = ((*m_viewMat) * m_modelMat).Invert().Transpose();

    // Create Mesh object.
    loadModel(vertices, indices, vertexLayout, matDefinition);

    // Create necessary constant buffers that are needed.
    initBuffers();
}


/*
 * ModelClass::Draw
 */
void ModelClass::Draw(bool depthPass) {
    // Update state information for the current frame.
    update();

    // ALWAYS Bind per-model constant buffer to slot 0. Contains model matrix etc.
    m_d3dContext->VSSetConstantBuffers(0, 1, m_constBuffer.GetAddressOf());

    // Loop over all meshes that define the model and draw them.
    for (unsigned int i = 0; i < m_meshes.size(); i++) {
        m_meshes[i].Draw(depthPass);
    }
}


/*
 * ModelClass::GetState
 */
ModelState* ModelClass::GetState() {
    return &m_state;
}


/*
 * ModelClass::processMesh
 */
Mesh ModelClass::processMesh(aiMesh* mesh, const aiScene* scene) {
    // Geometry of mesh.
    unsigned int vertexCnt = mesh->mNumVertices;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (unsigned int vertexIdx = 0; vertexIdx < vertexCnt; vertexIdx++) {
        Vertex vertex;

        // Process vertex positions.
        vertex.Position = dx::XMFLOAT3(mesh->mVertices[vertexIdx].x,
            mesh->mVertices[vertexIdx].y, mesh->mVertices[vertexIdx].z);

        // Process vertex normals. Skip this step if no normals are provided.
        if (mesh->HasNormals()) {
            vertex.Normal = dx::XMFLOAT3(mesh->mNormals[vertexIdx].x,
                mesh->mNormals[vertexIdx].y, mesh->mNormals[vertexIdx].z);
        } else {
            vertex.Normal = dx::XMFLOAT3(0.0f, 0.0f, 0.0f);
        }

        // Process vertex texture coordinates. Theoretically 8 coordinates per
        // vertex are possible. Here only one is considered.
        if (mesh->mTextureCoords[0]) { // does the mesh contain texture coordinates?
            vertex.TexCoords = dx::XMFLOAT2(mesh->mTextureCoords[0][vertexIdx].x,
                mesh->mTextureCoords[0][vertexIdx].y);
        } else {
            vertex.TexCoords = dx::XMFLOAT2(0.0f, 0.0f);
        }

        // Process tangents and bitangents.
        if (mesh->HasTangentsAndBitangents()) {
            vertex.Tangent = dx::XMFLOAT3(mesh->mTangents[vertexIdx].x,
                mesh->mTangents[vertexIdx].y, mesh->mTangents[vertexIdx].z);

            vertex.Bitangent = dx::XMFLOAT3(mesh->mBitangents[vertexIdx].x,
                mesh->mBitangents[vertexIdx].y, mesh->mBitangents[vertexIdx].z);

        } else {
            throw std::invalid_argument("Not implemented.");
        }

        vertices.push_back(vertex);
    }

    // Process indices. Mesh is given by array of faces.
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    // Process materials (textures).
    // A mesh only uses a single material (defined by constants and textures)!
    std::vector<Texture> textures;
    Material matDefinition;
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        // Ambient (map_Ka).
        std::vector<Texture> ambientMaps = loadMaterialTextures(material,
            aiTextureType_AMBIENT, "texture_ambient");
        textures.insert(textures.end(), ambientMaps.begin(), ambientMaps.end());

        // Diffuse (map_Kd).
        std::vector<Texture> diffuseMaps = loadMaterialTextures(material,
            aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

        // Specular (map_Ks).
        std::vector<Texture> specularMaps = loadMaterialTextures(material,
            aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

        // Normal (map_Kn).
        std::vector<Texture> normalMaps = loadMaterialTextures(material,
            aiTextureType_NORMALS, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

        // Bump (map_bump).
        std::vector<Texture> bumpMaps = loadMaterialTextures(material,
            aiTextureType_HEIGHT, "texture_bump");
        textures.insert(textures.end(), bumpMaps.begin(), bumpMaps.end());

        // Shininess/Dissolve (map_d).
        std::vector<Texture> dissolveMaps = loadMaterialTextures(material,
            aiTextureType_SHININESS, "texture_dissolve");
        textures.insert(textures.end(), dissolveMaps.begin(), dissolveMaps.end());

        // Dissolve (map_Ke).
        std::vector<Texture> emissiveMaps = loadMaterialTextures(material,
            aiTextureType_EMISSIVE, "texture_emissive");
        textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());

        // Load material constants. TODO: Make matColorViaTex more flexible.
        matDefinition = loadMaterial(material);
        matDefinition.matColorViaTex = (textures.size() > 0) ? true : false;
    }

    // Define vertex layout.
    std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // Second entry (0) defines semantic index --> TEXCOORD0

        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // D3D11_APPEND_ALIGNED_ELEMENT for automatic packing
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // Second entry (0) defines semantic index --> TEXCOORD0
        {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 } // Second entry (0) defines semantic index --> TEXCOORD0
    };

    // Return a configured Mesh object.
    return Mesh(vertices, indices, vertexLayout, textures, matDefinition, m_vertexShaderName,
        m_pixelShaderName,m_d3dDevice, m_d3dContext);
}


/*
 * ModelClass::loadMaterialTextures
 */
std::vector<Texture> ModelClass::loadMaterialTextures(
        aiMaterial* mat,
        aiTextureType type,
        std::string typeName) {
    // Loop over all contained textures.
    std::vector<Texture> textures;
    for (unsigned int texIdx = 0; texIdx < mat->GetTextureCount(type); texIdx++) {
        // Get texture informatin from Assimp.
        aiString str;
        mat->GetTexture(type, texIdx, &str);
        bool skip = false;

        // Compare texture name to all already-loaded texture.
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            // If texture has already been loaded, re-use the information from
            // the textures_loaded vector.
            if ((std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                && (std::strcmp(textures_loaded[j].type.data(), typeName.c_str()) == 0)) {
                // Re-use entry (path and texture type COMBINATION).
                textures.push_back(textures_loaded[j]);

                // Skip standard creation of texture.
                skip = true;
                break;
            }
        }

        // If the texture is new and not contained in textures_loaded, load it.
        if (!skip) {
            Texture texture;

            // Load texture and create SRV for it.
            std::wstring texPath = Helper::ConvertUtf8ToWide(
                m_directory + str.C_Str());
            if (m_fileFormat == 0) {
                HRESULT hr = dx::CreateWICTextureFromFile(m_d3dDevice.Get(),
                    m_d3dContext.Get(), texPath.c_str(), nullptr,
                    texture.srv.GetAddressOf());
                assert(SUCCEEDED(hr));
            } else {
                HRESULT hr = dx::CreateDDSTextureFromFile(m_d3dDevice.Get(),
                    m_d3dContext.Get(), texPath.c_str(), nullptr,
                    texture.srv.GetAddressOf());
                assert(SUCCEEDED(hr));
            }

            // Setup other fields.
            texture.type = typeName;            // Diffuse, specular, normal, ...
            texture.path = str.C_Str();         // Path to texture.
            textures.push_back(texture);        // Store Texture in vector of Mesh.
            textures_loaded.push_back(texture); // Add to textures_loaded.
        }
    }

    // Return vector of Textures for the current mesh.
    return textures;
}


/*
 * ModelClass::loadMaterial
 */
Material ModelClass::loadMaterial(aiMaterial* mat) {
    // Create new Material object.
    Material material = {};
    aiColor3D color(0.f, 0.f, 0.f);
    float shininess;

    mat->Get(AI_MATKEY_COLOR_AMBIENT, color);
    material.matAmbientColor = sm::Vector3(color.r, color.b, color.g);

    mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
    material.matDiffuseColor = sm::Vector3(color.r, color.b, color.g);

    mat->Get(AI_MATKEY_COLOR_SPECULAR, color);
    material.matSpecularColor = sm::Vector3(color.r, color.b, color.g);

    // Load shininess. TODO: Switch to updated assimp and remove scaling.
    // https://github.com/assimp/assimp/issues/123
    mat->Get(AI_MATKEY_SHININESS, shininess);
    material.matShininess = shininess / 4.0f;

    // TODO: Read from model for PBR etc..
    material.matDissolveFactor = 1.0;
    material.matOpticalDensity = 1.0;

    return material;
}


/*
 * ModelClass::computeNormalMatrix
 */
sm::Matrix ModelClass::computeNormalMatrix(
        const sm::Matrix& modelMatrix,
        const sm::Matrix& viewMatrix) {
    // Compute normal matrix.
    sm::Matrix modelViewMatrix = modelMatrix * viewMatrix;
    sm::Matrix normalMatrix4x4 = modelViewMatrix.Invert().Transpose();

    // Extract relevant 3x3 part, which is the only thing we need.
    sm::Matrix normalMatrix3x3;
    normalMatrix3x3._11 = normalMatrix4x4._11;
    normalMatrix3x3._12 = normalMatrix4x4._12;
    normalMatrix3x3._13 = normalMatrix4x4._13;

    normalMatrix3x3._21 = normalMatrix4x4._21;
    normalMatrix3x3._22 = normalMatrix4x4._22;
    normalMatrix3x3._23 = normalMatrix4x4._23;

    normalMatrix3x3._31 = normalMatrix4x4._31;
    normalMatrix3x3._32 = normalMatrix4x4._32;
    normalMatrix3x3._33 = normalMatrix4x4._33;

    return normalMatrix3x3;
}


/*
 * ModelClass::update
 */
void ModelClass::update() {
    // Re-compute model matrix.
    m_modelMat = sm::Matrix::CreateScale(m_state.scale)
        * sm::Matrix::CreateFromQuaternion(m_state.orientation)
        * sm::Matrix::CreateTranslation(m_state.position);

    // Pre-compute the normal matrix.
    m_normalMat = computeNormalMatrix(m_modelMat, *m_viewMat);

    // Upload updated state of model to GPU.
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        m_d3dContext->Map(m_constBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
        VS_PER_MODEL_CONSTANT_BUFFER* dataPtr = 
            (VS_PER_MODEL_CONSTANT_BUFFER*)mappedResource.pData;
        dataPtr->modelMat = m_modelMat.Transpose();
        dataPtr->normalMat = m_normalMat.Transpose();
        m_d3dContext->Unmap(m_constBuffer.Get(), 0);
    }
}


/*
 * ModelClass::initBuffers
 */
void ModelClass::initBuffers() {
    // Create constant buffer.
    VS_PER_MODEL_CONSTANT_BUFFER constBufferData;
    constBufferData.modelMat = m_modelMat.Transpose();

    // Fill in a buffer description.
    D3D11_BUFFER_DESC cbDesc;
    cbDesc.ByteWidth = sizeof(VS_PER_MODEL_CONSTANT_BUFFER);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;
    cbDesc.StructureByteStride = 0;

    // Fill in the subresource data.
    D3D11_SUBRESOURCE_DATA constInitData;
    constInitData.pSysMem = &constBufferData;
    constInitData.SysMemPitch = 0;
    constInitData.SysMemSlicePitch = 0;

    // Create the buffer.
    HRESULT hr = m_d3dDevice->CreateBuffer(&cbDesc, &constInitData,
        m_constBuffer.GetAddressOf());
    assert(SUCCEEDED(hr));
}


/*
 * ModelClass::initShadows
 */
void ModelClass::initShadows() {
    // TODO:
}


/*
 * ModelClass::createCubeMesh
 */
void ModelClass::createCubeMesh(sm::Vector3 color, bool usesInstancing) {
    // Define cube geometry.
    std::vector<Vertex> vertices = {
    // Back face.
    Vertex(sm::Vector3(-0.5f, -0.5f, -0.5f), sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(1.0f, 1.0f)), // 0
    Vertex(sm::Vector3(0.5f, -0.5f, -0.5f),  sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(0.0f,1.0f) ), // 1
    Vertex(sm::Vector3(-0.5f, 0.5f, -0.5f),  sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(1.0f,0.0f) ), // 2
    Vertex(sm::Vector3(0.5f, 0.5f, -0.5f),   sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(0.0f,0.0f) ), // 3
                                                                            
    // Front face.                                                          
    Vertex(sm::Vector3(-0.5f, -0.5f, 0.5f),  sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(1.0f,1.0f)), // 4
    Vertex(sm::Vector3(0.5f, -0.5f, 0.5f),   sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(0.0f,1.0f)), // 5
    Vertex(sm::Vector3(-0.5f, 0.5f, 0.5f),   sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(1.0f,0.0f)), // 6
    Vertex(sm::Vector3(0.5f, 0.5f, 0.5f),    sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(0.0f,0.0f)), // 7
                                                                            
    // Bottom face.                                                         
    Vertex(sm::Vector3(-0.5f, -0.5f, -0.5f), sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(1.0f,1.0f)), // 8
    Vertex(sm::Vector3(0.5f, -0.5f, -0.5f),  sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(0.0f,1.0f)), // 9
    Vertex(sm::Vector3(-0.5f, -0.5f, 0.5f),  sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(1.0f,0.0f)), // 10
    Vertex(sm::Vector3(0.5f, -0.5f, 0.5f),   sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(0.0f,0.0f)), // 11
                                                                            
    // Top face.                                                            
    Vertex(sm::Vector3(-0.5f, 0.5f, -0.5f),  sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(1.0f,1.0f)), // 12
    Vertex(sm::Vector3(0.5f, 0.5f, -0.5f),   sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(0.0f,1.0f)), // 13
    Vertex(sm::Vector3(-0.5f, 0.5f, 0.5f),   sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(1.0f,0.0f)), // 14
    Vertex(sm::Vector3(0.5f, 0.5f, 0.5f),    sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(0.0f,0.0f)), // 15
                                                                            
    // Left face.                                                           
    Vertex(sm::Vector3(-0.5f, -0.5f, -0.5f), sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(1.0f,1.0f)), // 16
    Vertex(sm::Vector3(-0.5f, 0.5f, -0.5f),  sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(0.0f,1.0f)), // 17
    Vertex(sm::Vector3(-0.5f, -0.5f, 0.5f),  sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(1.0f,0.0f)), // 18
    Vertex(sm::Vector3(-0.5f, 0.5f, 0.5f),   sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(0.0f,0.0f)), // 19
                                                                            
    // Right face.                                                          
    Vertex(sm::Vector3(0.5f, -0.5f, -0.5f),  sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(1.0f,1.0f)), // 20
    Vertex(sm::Vector3(0.5f, 0.5f, -0.5f),   sm::Vector3(1.0f, 1.0f, 1.0f), sm::Vector2(0.0f,1.0f)), // 21
    Vertex(sm::Vector3(0.5f, -0.5f, 0.5f),   sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(1.0f,0.0f)), // 22
    Vertex(sm::Vector3(0.5f, 0.5f, 0.5f),    sm::Vector3(0.0f, 0.0f, 0.0f), sm::Vector2(0.0f,0.0f)), // 23
    };

    // Define cube faces.
    std::vector<unsigned int> indices = {
        0, 1, 2,  1, 3, 2,  // back
        5, 4, 7,  4, 6, 7,  // front
        9,8,11,  8,10,11,   //bottom
        15,14,13, 14,12,13, // top
        18,16,19, 16,17,19, // left
        20,22,21, 22,23,21  //right
    };

    // Create material definition.
    Material matDefinition;
    matDefinition.matAmbientColor = color;
    matDefinition.matDiffuseColor = color;
    matDefinition.matSpecularColor = color;
    matDefinition.matShininess = 1.0f;
    matDefinition.matOpticalDensity = 1.0f;
    matDefinition.matDissolveFactor = 1.0f;
    matDefinition.matColorViaTex = false;   // Tell pixel shader where color is.
    matDefinition.matPadding0 = 0.0f;
    matDefinition.matPadding1 = 0.0f;
    matDefinition.matPadding2 = 0.0f;

    std::vector<Texture> textures;
    std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout =
        createVertexInputLayout(usesInstancing);
    
    // Store the single mesh of this ModelClass object.
    m_meshes.push_back(Mesh(
        vertices,
        indices,
        vertexLayout,
        textures,
        matDefinition,
        m_vertexShaderName,
        m_pixelShaderName,
        m_d3dDevice,
        m_d3dContext));
}


/*
 * ModelClass::createSphereMesh
 */
void ModelClass::createSphereMesh(sm::Vector3 color, bool usesInstancing) {
    const int resTheta = 128;
    const int resPhi = 64;
    const double radiusSphere = 0.5;
    constexpr float pi = 3.14159265358979323846f;  // TODO: Use lib.

    // Vectors holding sphere vertex/uv/index information.
    std::vector<Vertex> vertices;
    std::vector<float> texCoords;
    std::vector<unsigned int> indices;

    // Vertex-coords and uv-coords.
    float x, y, z;
    float u, v;

    // Renaming for easier understanding.
    int verticalResolution = resTheta;
    int horizontalResolution = resPhi;

    // Step sizes and angles for vertices calculation.
    float verticalStep = DirectX::XMConvertToRadians(
        180.0f / float(verticalResolution));
    float horizontalStep = DirectX::XMConvertToRadians(
        360.0f / float(horizontalResolution));
    float verticalAngle = 0.0f;
    float horizontalAngle = 0.0f;

    //Calculate vertices by looping over angles.
    for (int i = 0; i <= horizontalResolution; i++) {
        horizontalAngle = i * horizontalStep;
        for (int j = 0; j <= verticalResolution; j++) {
            verticalAngle = j * verticalStep;

            // Calculate vertex position.
            x = radiusSphere * sin(verticalAngle) * cos(horizontalAngle);
            y = radiusSphere * sin(verticalAngle) * sin(horizontalAngle);
            z = radiusSphere * cos(verticalAngle);

            // Calculate texture coordinate.
            u = horizontalAngle / (2 * pi);
            v = verticalAngle / (pi);
            texCoords.push_back(u);
            texCoords.push_back(v);

            // Add to vertices. Position also acts as the normal.
            vertices.push_back(Vertex(sm::Vector3(x, y, z), sm::Vector3(x, y, z),
                sm::Vector2(u, v)));
        }
    }

    // Calculate indices. Similar to http://www.songho.ca/opengl/gl_sphere.html
    // Visualization:     (k1)----(k1+verticalResolution + 1)
    //                     |                   |
    //                     |                   |
    //                    (k2)----(k2+verticalResolution+1)
    int k1, k2;
    for (int i = 0; i < horizontalResolution; i++) {
        for (int j = 0; j < verticalResolution; j++) {
            k1 = i * (verticalResolution + 1) + j;
            k2 = k1 + 1;

            // Skip geographic north pole.
            if (j != 0) {
                // k1 => k2 => k1+1 vertex.
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + verticalResolution + 1);
            }

            // Skip geographic south pole.
            if (j != (verticalResolution - 1)) {
                // k1+1 => k2 => k2+1 vertex.
                indices.push_back(k1 + verticalResolution + 1);
                indices.push_back(k2);
                indices.push_back(k2 + verticalResolution + 1);
            }
        }
    }

    // Define materials.
    Material matDefinition;
    matDefinition.matAmbientColor = color;
    matDefinition.matDiffuseColor = color;
    matDefinition.matSpecularColor = color;
    matDefinition.matShininess = 1.0f;
    matDefinition.matOpticalDensity = 1.0f;
    matDefinition.matDissolveFactor = 1.0f;
    matDefinition.matColorViaTex = false;
    matDefinition.matPadding0 = 0.0f;
    matDefinition.matPadding1 = 0.0f;
    matDefinition.matPadding2 = 0.0f;

    // Store the single mesh of this ModelClass object.
    std::vector<Texture> textures;
    std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout = 
        createVertexInputLayout(usesInstancing);
    
    m_meshes.push_back(Mesh(
        vertices,
        indices,
        vertexLayout,
        textures,
        matDefinition,
        m_vertexShaderName,
        m_pixelShaderName,
        m_d3dDevice,
        m_d3dContext));
}


/*
 * ModelClass::createTorusMesh
 */
void ModelClass::createTorusMesh(sm::Vector3 color, bool usesInstancing) {
    const int resT = 64; // Toroidal    Big circle
    const int resP = 64; // Poloidal    Small circle
    const float radiusOut = 0.34f;
    const float radiusIn = 0.16f;
    constexpr float pi = 3.14159265358979323846f;  // TODO: Use lib.

    // Step sizes.
    constexpr float outCircleStep = DirectX::XMConvertToRadians(360.0f / float(resT));
    constexpr float inCircleStep = DirectX::XMConvertToRadians(360.0f / float(resP));

    // Vectors holding torus vertex/normal/params/index information.
    std::vector<Vertex> vertices;
    std::vector<float> normals;
    std::vector<unsigned int> indices;

    // Normal and angles for vertices calculation.
    float nx, ny, nz, normalLength;
    float currOutAngle = 0.0f;
    float currInAngle = 0.0f;

    // Calculate vertices of torus.
    for (int i = 0; i <= resT; i++) {
        currInAngle = 0.0f;
        for (int j = 0; j <= resP; j++) {
            // Add vertex to vertex list.
            float x = (radiusOut + radiusIn * cos(currInAngle)) * cos(currOutAngle);
            float y = (radiusOut + radiusIn * cos(currInAngle)) * sin(currOutAngle);
            float z = radiusIn * sin(currInAngle);

            // Add normals to normal list.
            nx = cos(currInAngle) * cos(currOutAngle);
            ny = sin(currOutAngle) * cos(currInAngle);
            nz = sin(currInAngle);
            normalLength = sqrt(pow(nx, 2) + pow(ny, 2) + pow(nz, 2));
            float normalX = nx / normalLength;
            float normalY = ny / normalLength;
            float normalZ = nz / normalLength;

            // Add step size to inner angle.
            currInAngle += inCircleStep;

            // Add to vertices. Position also acts as the normal.
            vertices.push_back(Vertex(sm::Vector3(x, y, z),
                sm::Vector3(normalX, normalY, normalZ),
                sm::Vector2(0, 0)));
        }

        // Add step size to outer angle.
        currOutAngle += outCircleStep;
    }

    // Calculate indices of torus.
    unsigned int currentVertexOffset = 0;
    for (auto i = 0; i < resT; i++) {
        for (auto j = 0; j <= resP; j++) {
            unsigned int vertexA = currentVertexOffset;
            indices.push_back(vertexA);

            unsigned int vertexB = currentVertexOffset + resP + 1;
            indices.push_back(vertexB);
            currentVertexOffset++;
        }
    }

    // Define materials.
    Material matDefinition;
    matDefinition.matAmbientColor = color;
    matDefinition.matDiffuseColor = color;
    matDefinition.matSpecularColor = color;
    matDefinition.matShininess = 1.0f;
    matDefinition.matOpticalDensity = 1.0f;
    matDefinition.matDissolveFactor = 1.0f;
    matDefinition.matColorViaTex = false;
    matDefinition.matPadding0 = 0.0f;
    matDefinition.matPadding1 = 0.0f;
    matDefinition.matPadding2 = 0.0f;

    // Store the single mesh of this ModelClass object.
    std::vector<Texture> textures;
    std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout =
        createVertexInputLayout(usesInstancing);
    
    m_meshes.push_back(Mesh(
        vertices,
        indices,
        vertexLayout,
        textures,
        matDefinition,
        m_vertexShaderName,
        m_pixelShaderName,
        m_d3dDevice,
        m_d3dContext));
}

std::vector<D3D11_INPUT_ELEMENT_DESC> ModelClass::createVertexInputLayout(bool usesInstancing) {
    std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout;
    vertexLayout = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // Second entry (0) defines semantic index --> TEXCOORD0

            // For normal mapping.
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // D3D11_APPEND_ALIGNED_ELEMENT for automatic packing
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // Second entry (0) defines semantic index --> TEXCOORD0
            {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 } // Second entry (0) defines semantic index --> TEXCOORD0
    };

    // For instanced rendering.
    if (usesInstancing) {
        // Position.
        D3D11_INPUT_ELEMENT_DESC posDesc;
        posDesc.SemanticName = "TEXCOORD";
        posDesc.SemanticIndex = 1;
        posDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
        posDesc.InputSlot = 1;
        posDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        posDesc.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
        posDesc.InstanceDataStepRate = 1;
        vertexLayout.push_back(posDesc);

        // Scale.
        D3D11_INPUT_ELEMENT_DESC scaleDesc;
        scaleDesc.SemanticName = "TEXCOORD";
        scaleDesc.SemanticIndex = 2;
        scaleDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
        scaleDesc.InputSlot = 1;
        scaleDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        scaleDesc.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
        scaleDesc.InstanceDataStepRate = 1;
        vertexLayout.push_back(scaleDesc);

        // Color.
        D3D11_INPUT_ELEMENT_DESC colorDesc;
        colorDesc.SemanticName = "TEXCOORD";
        colorDesc.SemanticIndex = 3;
        colorDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
        colorDesc.InputSlot = 1;
        colorDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        colorDesc.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
        colorDesc.InstanceDataStepRate = 1;
        vertexLayout.push_back(colorDesc);
    }

    return vertexLayout;
}


/*
 * ModelClass::processNode
 */
void ModelClass::processNode(aiNode* node, const aiScene* scene) {
    // Process all meshes of the node (if any).
    for (unsigned int meshIdx = 0; meshIdx < node->mNumMeshes; meshIdx++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIdx]];
        m_meshes.push_back(processMesh(mesh, scene));
    }

    // Do the same for each of its children.
    for (unsigned int childIdx = 0; childIdx < node->mNumChildren; childIdx++) {
        processNode(node->mChildren[childIdx], scene);
    }

    // TODO: Extend on parent-child relationship.
}


/*
 * ModelClass::loadModel
 */
void ModelClass::loadModel() {
    // Load the model. aiProcess_ValidateDataStructure is necessary for some models,
    // in order to avoid errors.
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(m_fullModelPath, 
        aiProcess_Triangulate | aiProcess_ValidateDataStructure | 
        aiProcess_JoinIdenticalVertices | m_modelExtraFlags);

    // Check for errors.
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        OutputDebugStringA("ASSIMP error:");
        OutputDebugStringA(importer.GetErrorString());
        assert(false);
    }

    // Start recursive processing.
    processNode(scene->mRootNode, scene);
}


/*
 * ModelClass::loadModel
 */
void ModelClass::loadModel(
        std::vector<Vertex> vertices,
        std::vector<unsigned int> indices,
        std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout,
        Material matDefinition) {
    // Store the single mesh of this ModelClass object.
    std::vector<Texture> textures = {};
    m_meshes.push_back(Mesh(vertices,
        indices,
        vertexLayout,
        textures,
        matDefinition,
        m_vertexShaderName,
        m_pixelShaderName,
        m_d3dDevice,
        m_d3dContext));
}
