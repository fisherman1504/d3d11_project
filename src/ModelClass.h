#pragma once
#include "Mesh.h"

/// <summary>
/// Defines the state of a ModelClass object.
/// </summary>
struct ModelState {
    float scale;
    sm::Vector3 position;
    sm::Quaternion orientation;

    // Constructor.
    ModelState(float scale, sm::Vector3 position, sm::Vector4 rotation) :
        scale(scale), position(position),
        orientation(sm::Quaternion(rotation)) {
        orientation.Normalize();
    };

    // Default constructor.
    ModelState() {
        scale = 1.0;
        position = sm::Vector3(0.0, 0.0, 0.0);
        orientation = sm::Quaternion::Identity;
        orientation.Normalize();
    }
};

/// <summary>
/// Represents a complex model, that consists of multiple meshes.
/// </summary>
/// <remarks>
/// Heavily inspired by https://learnopengl.com/Model-Loading/Model
/// </remarks>
class ModelClass {
public:
    /// <summary>
    /// Indicates where the (mesh) data for the ModelClass object came from.
    /// </summary>
    enum class BaseType {
        TRIANGLE,   // Defined in this class.
        CUBE,       // Defined in this class.
        SPHERE,     // Defined in this class.
        TORUS,      // Defined in this class.
        LOADED,     // Mesh defined via file on disk.
        CUSTOM      // Mesh defined outside and passed in via constructor.
    };

    /// <summary>
    /// Constructor that is used when loading a model from disk. Currently made for 
    /// sponza model.
    /// </summary>
    /// <param name="directory">Directory of the model on disk.</param>
    /// <param name="name">Name of the model.</param>
    /// <param name="modelExtraFlags">Extra flags for assimp parsing.</param>
    /// <param name="d3dDevice">D3D11 device pointer.</param>
    /// <param name="d3dContext">D3D11 context pointer.</param>
    /// <param name="viewMat">View matrix of main camera.</param>
    /// <param name="initScale">Scale of the object. Used for modelMat construction.
    /// </param>
    /// <param name="initPosition">Initial position of the object. Used for modelMat
    ///  construction.</param>
    /// <param name="initRotation">Initial rotation of the object. Used for modelMat
    ///  construction.</param>
    /// <param name="fileFormat">0 = .obj (wavefront), 1 = .dae (Collada)</param>
    ModelClass(
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
        std::wstring pixelShaderName);

    /// <summary>
    /// Constructor that is used when creating a model for a pre-defined mesh (cube,
    ///  sphere, ...).
    /// </summary>
    /// <param name="modelType">BaseType that describes the mesh.</param>
    /// <param name="color">Color used for the model.</param>
    /// <param name="d3dDevice">D3D11 device pointer.</param>
    /// <param name="d3dContext">D3D11 context pointer.</param>
    /// <param name="viewMat">View matrix of main camera.</param>
    /// <param name="initScale">Scale of the object. Used for modelMat construction.
    /// </param>
    /// <param name="initPosition">Initial position of the object. Used for modelMat
    ///  construction.</param>
    /// <param name="initRotation">Initial rotation of the object. Used for modelMat
    ///  construction.</param>
    /// <param name="vertexShaderName">Vertex shader for this model.</param>
    /// <param name="pixelShaderName">Pixel shader for this model.</param>
    ModelClass(
        BaseType modelType,
        sm::Vector3 color,
        wrl::ComPtr<ID3D11Device> d3dDevice,
        wrl::ComPtr<ID3D11DeviceContext> d3dContext,
        const sm::Matrix* viewMat,
        float initScale,
        sm::Vector3 initPosition,
        sm::Vector4 initRotation,
        std::wstring vertexShaderName,
        std::wstring pixelShaderName);

    /// <summary>
    /// Will render a BaseType mesh multiple times via instanced rendering.
    /// </summary>
    /// <param name="modelType"></param>
    /// <param name="positions"></param>
    /// <param name="colors"></param>
    /// <param name="scales"></param>
    /// <param name="rotations"></param>
    /// <param name="d3dDevice"></param>
    /// <param name="d3dContext"></param>
    /// <param name="viewMat"></param>
    /// <param name="vertexShaderName"></param>
    /// <param name="pixelShaderName"></param>
    ModelClass(
        BaseType modelType,
        std::vector<sm::Vector3> positions,
        std::vector<sm::Vector3> colors,
        std::vector<sm::Vector3> scales,
        std::vector <sm::Vector4> rotations,
        wrl::ComPtr<ID3D11Device> d3dDevice,
        wrl::ComPtr<ID3D11DeviceContext> d3dContext,
        const sm::Matrix* viewMat,
        std::wstring vertexShaderName,
        std::wstring pixelShaderName);

    /// <summary>
    /// Constructor that is used when passing in mesh data from the outside.
    /// </summary>
    /// <param name="vertices">Vertex data.</param>
    /// <param name="indices">Indices for vertex data.</param>
    /// <param name="vertexLayout">Vertex data layout.</param>
    /// <param name="matDefinition">Material for the whole mesh.</param>
    /// <param name="d3dDevice">D3D11 device pointer.</param>
    /// <param name="d3dContext">D3D11 context pointer.</param>
    /// <param name="viewMat">View matrix of main camera.</param>
    /// <param name="initScale">Scale of the object. Used for modelMat construction.
    /// </param>
    /// <param name="initPosition">Initial position of the object. Used for modelMat
    /// construction.</param>
    /// <param name="initRotation">Initial rotation of the object. Used for modelMat
    /// construction.</param>
    /// <param name="vertexShaderName">Vertex shader for this model.</param>
    /// <param name="pixelShaderName">Pixel shader for this model.</param>
    ModelClass(
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
        std::wstring pixelShaderName);

    /// <summary>
    /// Draw the model to the currently bound framebuffer.
    /// </summary>
    /// <param name="depthPass">Set true if no framebuffer is bound. Used only in the
    /// first phase of shadow mapping.</param>
    void Draw(bool depthPass);
    
    /// <summary>
    /// Returns pointer to the state of the model (position, rotation, scale).
    /// </summary>
    /// <returns></returns>
    ModelState* GetState();

private:
    /// <summary>
    /// Load a model from disk.
    /// </summary>
    void loadModel();

    /// <summary>
    /// Load a model defined by vertices and indices.
    /// </summary>
    /// <param name="vertices">Vertex data.</param>
    /// <param name="indices">Indices for vertex data.</param>
    /// <param name="vertexLayout">Vertex data layout.</param>
    /// <param name="matDefinition">Material for the whole mesh.</param>
    void loadModel(std::vector<Vertex> vertices,
        std::vector<unsigned int> indices,
        std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout,
        Material matDefinition);

    /// <summary>
    /// Processes a node in an assimp graph.
    /// </summary>
    /// <param name="node">Pointer to the node of the graph.</param>
    /// <param name="scene">Current assimp scene.</param>
    void processNode(aiNode* node, const aiScene* scene);

    /// <summary>
    /// Process mesh of an assimp node.
    /// </summary>
    /// <param name="mesh">Pointer to the mesh.</param>
    /// <param name="scene">Current assimp scene.</param>
    /// <returns>Created Mesh object.</returns>
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);

    /// <summary>
    /// Loads textures from disk and stores them efficiently.
    /// </summary>
    /// <param name="mat">Pointer to the material.</param>
    /// <param name="type">Type of the texture.</param>
    /// <param name="typeName">Name of the texture.</param>
    /// <returns></returns>
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type,
        std::string typeName);

    /// <summary>
    /// Get and set material information.
    /// </summary>
    /// <param name="mat">The material where the information is contained in.</param>
    /// <returns>Material object.</returns>
    Material loadMaterial(aiMaterial* mat);

    /// <summary>
    /// Called every frame. Updates all relevant resources.
    /// </summary>
    void update();
    
    /// <summary>
    /// Creates relevant buffers of a ModelClass object.
    /// </summary>
    void initBuffers();

    /// <summary>
    /// Creates relevant resources for shadow mapping.
    /// </summary>
    void initShadows();

    /// <summary>
    /// Creates mesh data for a cube.
    /// </summary>
    /// <param name="color">Color of the cube.</param>
    void createCubeMesh(sm::Vector3 color, bool usesInstancing);

    /// <summary>
    /// Creates mesh data for a sphere.
    /// </summary>
    /// <param name="color">Color of the sphere.</param>
    void createSphereMesh(sm::Vector3 color, bool usesInstancing);

    /// <summary>
    /// Creates mesh data for a torus (donut).
    /// </summary>
    /// <param name="color">Color of the torus.</param>
    void createTorusMesh(sm::Vector3 color, bool usesInstancing);

    /// <summary>
    /// Returns a generic layout.
    /// </summary>
    /// <returns></returns>
    std::vector<D3D11_INPUT_ELEMENT_DESC> createVertexInputLayout(bool usesInstancing);

    /// <summary>
    /// Computes the normal matrix.
    /// </summary>
    /// <param name="modelMatrix">Model matrix of the ModelClass object.</param>
    /// <param name="viewMatrix">View matrix of the ModelClass object.</param>
    /// <returns></returns>
    sm::Matrix computeNormalMatrix(
        const sm::Matrix& modelMatrix,
        const sm::Matrix& viewMatrix);

    // Data for models that were loaded from disk.
    std::string m_directory;
    std::string m_name;
    std::string m_fullModelPath;
    unsigned int m_modelExtraFlags; // For assimp loading.
    unsigned int m_fileFormat;

    // All the meshes and textures that define the model.
    std::vector<Mesh> m_meshes;
    std::vector<Texture> textures_loaded;

    // D3D11 information.
    wrl::ComPtr<ID3D11Device> m_d3dDevice;
    wrl::ComPtr<ID3D11DeviceContext> m_d3dContext;

    // Constant buffer that all models have bound to the vertex shader.
    struct VS_PER_MODEL_CONSTANT_BUFFER {
        sm::Matrix modelMat;
        sm::Matrix normalMat;
    };
    wrl::ComPtr<ID3D11Buffer> m_constBuffer;

    // Important information about the model.
    ModelState m_state;
    const sm::Matrix* m_viewMat;
    sm::Matrix m_modelMat;      // Combination of scale, position and rotation.
    sm::Matrix m_normalMat;
    std::wstring m_vertexShaderName;
    std::wstring m_pixelShaderName;

    /// <summary>
    /// Indicates what type of model is being rendered.
    /// If it is ModelClass::BaseType::LOADED, then up to 7 textures will be bound
    /// in the pixel shader stage since the model was loaded from a file from disk.
    /// </summary>
    ModelClass::BaseType m_baseType;

    // Instanced rendering. Adapted from:
    // https://www.rastertek.com/dx11tut37.html
    __declspec(align(16)) struct InstanceType {
        sm::Vector3 iPos;
        sm::Vector3 iScale;
        sm::Vector3 iColor;
    };
    unsigned int m_instanceCount;
    unsigned int m_instanceStride;
    wrl::ComPtr<ID3D11Buffer> m_instanceBuffer;
};