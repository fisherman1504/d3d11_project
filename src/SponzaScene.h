#pragma once
#include "Scene.h"
#include "ModelClass.h"

// ImGui.
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

/// <summary>
/// Sponza scene. Life is pain.
/// </summary>
class SponzaScene : public Scene {
public:
	/// <summary>
	/// Inherit constructors.
	/// </summary>
	using Scene::Scene;

	/// <inheritdoc />
	virtual void Render(
		wrl::ComPtr<ID3D11RenderTargetView>	d3dFrameBufferView,
		wrl::ComPtr<ID3D11DepthStencilView>	d3dFrameBufferDepthStencilView)
		override;

	/// <inheritdoc />
	virtual void Init() override;

protected:
	/// <inheritdoc />
	virtual void initModels() override;

	/// <inheritdoc />
	virtual void resizeEvent() override;

private:
	/// <summary>
	/// Defines the scene-specific GUI.
	/// </summary>
	void defineImGui();

	/// <summary>
	/// Called once per frame. Updates relevant resources.
	/// </summary>
	void update();

	/// <summary>
	/// Updates all parameters related to the camera.
	/// </summary>
	void updateCamera();

	/// <summary>
	/// Updates all buffers on the GPU.
	/// </summary>
	void updateBuffers();

	/// <summary>
	/// Updates state of all models in the scene.
	/// </summary>
	void updateModels();

	/// <summary>
	/// Updates matrices for texture visualization quad.
	/// </summary>
	void updateTexVisQuad();

	/// <summary>
	/// Computes view and orthographic projection matrix for the directional light,
	/// which causes shadows. Dynamically adapts light frustum to camera frustum.
	/// </summary>
	/// <remarks>
	/// MJP comment:
	/// https://gamedev.net/forums/topic/505893-orthographic-projection-for-shadow-mapping/505893/
	/// </remarks>
	void updateShadows();

	/// <summary>
	/// Updates the model and projection matrix that is used for rendering the
	/// window-filling quad (adaptive to viewport size).
	/// </summary>
	void updateLightingPass();

	/// <summary>
	/// Updates resources that are being used for SSAO.
	/// </summary>
	void updateSSAO();

	/// <summary>
	/// Init the GBuffer textures and views.
	/// </summary>
	void initGBuffer();

	/// <summary>
	/// Init all resources for shadow mapping.
	/// </summary>
	/// <remark>
	/// https://learn.microsoft.com/en-us/windows/uwp/gaming/implementing-depth-buffers-for-shadow-mapping
	/// </remark>
	void initShadows();

	/// <summary>
	/// Init all resources for shadow mapping that depend on the screen resolution.
	/// </summary>
	void initShadowTextures();

	/// <summary>
	/// Creates all lights in the scene.
	/// </summary>
	void initLights();

	/// <summary>
	/// Init all resources for the texture visualization quad.
	/// </summary>
	void initTextureVisualization();

	/// <summary>
	/// Init window-filling quad resources for the lighting pass.
	/// </summary>
	void initLightingPass();

	/// <summary>
	/// Init scene related buffers.
	/// </summary>
	void initSceneBuffers();

	/// <summary>
	/// Init skybox related resources (cube model and cube map).
	/// </summary>
	void initSkyBox();

	/// <summary>
	/// Init all SSAO related resources.
	/// </summary>
	void initSSAO();

	/// <summary>
	/// Init the main occlusion texture for SSAO.
	/// </summary>
	void initSSAOOccMaps();

	/// <summary>
	/// Init query buffer for performance profiling.
	/// </summary>
	void initProfiling();

	/// <summary>
	/// Updates profiling stats.
	/// </summary>
	void processPerformanceMetrics();

	/// <summary>
	/// Creates a timestamp query.
	/// </summary>
	/// <param name="device"></param>
	/// <returns></returns>
	wrl::ComPtr <ID3D11Query> createTimestampQuery(
		wrl::ComPtr<ID3D11Device> device);

	/// <summary>
	/// Creates a disjoint query.
	/// </summary>
	/// <param name="device"></param>
	/// <returns></returns>
	wrl::ComPtr <ID3D11Query> createDisjointQuery(
		wrl::ComPtr<ID3D11Device> device);

	/// <summary>
	/// Linear interpolation.
	/// </summary>
	/// <param name="a"></param>
	/// <param name="b"></param>
	/// <param name="f"></param>
	/// <returns></returns>
	float lerp(float a, float b, float f);

	// Camera.
	sm::Matrix m_viewMat;
	sm::Matrix m_projMat;
	float m_cameraPitch;
	float m_cameraYaw;

	// TODO: Improve so z-Buffers 0-1 range gets filled out as much as possible.
	float m_cameraNearClip = 3.0;	// To have nicely filled-out z-Buffer.
	float m_cameraFarClip = 300.0;	// Sponza scene diameter is around 300.
	sm::Vector3 m_viewPos;
	sm::Vector3 m_cameraLookAt;
	const sm::Vector3 START_POSITION = { 0.0f, 0.0f, -6.0f };
	const float ROTATION_GAIN = 0.04f;
	const float MOVEMENT_GAIN = 0.37f;

	// Scene ModelClass objects.
	std::shared_ptr <ModelClass> m_sponzaModel;
	std::shared_ptr <ModelClass> m_originVisualization;
	std::shared_ptr <ModelClass> m_pointLightVisualization;
	std::shared_ptr <ModelClass> m_texVisQuad;
	std::shared_ptr <ModelClass> m_skyBoxCube;
	std::shared_ptr <ModelClass> m_lightVolumes;

	// GUI variables.
	bool useAnimation;
	float m_modelYaw;
	float m_modelPitch;
	float m_modelRoll;
	bool m_showWireframe;
	bool m_showTexVis = false;	// Turn on/off texture visualization.
	bool m_showOriginVis = false;

	// GUI: Select what should be shown on screen.
	std::array<std::string, 7> DrawModeStrings = {
		"BLINN_PHONG",
		"AMBIENT_TERM",
		"DIFFUSE_TERM",
		"SPECULAR_TERM",
		"VIEW_SPACE_NORMALS",
		"WORLD_SPACE_POS",
		"W/O ALBEDO/DIFFUSE"
		};
	int m_drawMode;

	// GUI: Select what texture should be shown in the visualization.
	std::array<std::string, 6> TexVisStrings = {
		"GBUFFER_DEPTH",
		"DIR_LIGHT_DEPTH",
		"POINT_LIGHT_DIFFUSE",
		"POINT_LIGHT_SPECULAR",
		"OCCLUSION_MAP",
		"OCCLUSION_MAP_BLURRED" };
	unsigned int m_texVisTextureIdx = 0;	// Register index of shown texture.

	/// <summary>
	/// Contents of the sponza scene VS cbuffer.
	/// </summary>
	struct VS_CONSTANT_BUFFER {
		sm::Matrix viewMat;
		sm::Matrix projMat;
		sm::Vector3 viewPos;		// World space.
		float padding0;

		sm::Matrix lightViewMat;
		sm::Matrix lightProjMat;
	};

	/// <summary>
	/// VS Constant Buffer of the Sponza scene.
	/// </summary>
	wrl::ComPtr<ID3D11Buffer> m_sceneBufferVS;

	/// <summary>
	/// Contents of the sponza scene PS cbuffer.
	/// </summary>
	struct PS_CONSTANT_BUFFER {
		sm::Vector3 lightingScales;
		float shininessExp;

		// For visualization of normals in view space (matches sponza reference).
		sm::Matrix viewMat;

		sm::Matrix projMat;

		// For light volumes.
		sm::Matrix invViewProjMat;

		// Other information.
		int drawMode;
		sm::Vector3 viewPos;	// World space.
		sm::Vector4 pixelSize;
	};

	/// <summary>
	/// PS Constant Buffer of the Sponza scene.
	/// </summary>
	wrl::ComPtr<ID3D11Buffer> m_sceneBufferPS;

	// Rasterizer state for the scene. Used for toggling wirframe mode on/off.
	D3D11_RASTERIZER_DESC m_rasterDesc;
	wrl::ComPtr<ID3D11RasterizerState> m_rasterizerState;

	// Directional Light. Causes shadows.
	std::array<unsigned int, 5> m_shadowMapSizes = { 512,1024,2048,4096,8192 };
	unsigned int m_shadowMapSizeIdx = 3;
	bool m_useShadows = true;
	std::array<std::string, 2> m_shadowTypes = {
	"HARD SHADOWS",
	"PCF SOFT SHADOWS"};
	unsigned int m_shadowTypeIdx = 1;

	wrl::ComPtr<ID3D11Texture2D> m_shadowMap;
	wrl::ComPtr<ID3D11DepthStencilView> m_shadowDepthView;
	wrl::ComPtr<ID3D11ShaderResourceView> m_shadowResourceView;
	wrl::ComPtr<ID3D11SamplerState> m_comparisonSampler_point;
	wrl::ComPtr<ID3D11RasterizerState> m_rasterizerStateShadows;
	wrl::ComPtr<ID3D11DepthStencilState> m_shadowDepthStencilState;
	D3D11_VIEWPORT m_shadowViewport;
	sm::Vector3 m_directionalLightPos = { 0.0,500.0,10.0 };
	sm::Matrix m_directionalLightProjectionMat;		// Orthographic projection.
	sm::Matrix m_directionalLightViewMat;			// Light view space.

	/// <summary>
	/// Contents of the VS cbuffer that contains the matrices for shadow mapping.
	/// </summary>
	struct VS_SHADOW_CONSTANT_BUFFER {
		sm::Matrix lightViewMat;
		sm::Matrix lightProjMat;
	};

	/// <summary>
	/// VS cbuffer that contains the matrices for shadow mapping. Only used by
	/// ShadowVS.hlsl.
	/// </summary>
	wrl::ComPtr<ID3D11Buffer> m_shadowConstBufferVS;

	/// <summary>
	/// Contents of the PS cbuffer that contains important shadow mapping info.
	/// </summary>
	struct PS_SHADOW_CONSTANT_BUFFER {
		sm::Vector3 lightDir;
		unsigned int shadowMapSize;
		sm::Matrix lightViewMat;	// TODO: Remove by adding extra shadow map pass.
		sm::Matrix lightProjMat;	// TODO: Remove by adding extra shadow map pass.
		unsigned int shadowType;	// Hard shadows, PCF, ...
		BOOL useShadows;
		unsigned int padding1;
		unsigned int padding2;
	};

	/// <summary>
	/// PS cbuffer that contains the light direction etc. for shadow mapping. Only
	/// used by Sponza_ps.hlsl.
	/// </summary>
	wrl::ComPtr<ID3D11Buffer> m_shadowConstBufferPS;

	// Texture visualization quad.
	sm::Matrix m_texVisModelMat;
	sm::Matrix m_texVisProjMat;

	/// <summary>
	/// Contents of the VS cbuffer that contains matrices for the visualization quad.
	/// </summary>
	struct VS_MP_BUFFER {
		sm::Matrix modelMat;
		sm::Matrix projectionMat;
	};

	/// <summary>
	/// VS cbuffer for rendering the texture visualization quad.
	/// </summary>
	wrl::ComPtr<ID3D11Buffer> m_texVisConstBuffer;

	/// <summary>
	/// Contents of the VS cbuffer that stores the selected texture index for
	/// visualization.
	/// </summary>
	struct PS_TexVis_BUFFER {
		unsigned int textureIdx;
		sm::Vector3 padding3;
	};

	/// <summary>
	/// PS cbuffer for rendering the texture visualization quad.
	/// </summary>
	wrl::ComPtr<ID3D11Buffer> m_texVisPSBuffer;

	// Skybox cube map.
	bool m_useSkyBox = true;
	Texture m_skyBoxTexture;

	// Window-filling quad.
	std::shared_ptr <ModelClass> m_lightingPassQuad;
	sm::Matrix m_lightingPassQuadModelMat;
	sm::Matrix m_lightingPassQuadProjMat;

	/// <summary>
	/// VS_MP_BUFFER cbuffer for the window-filling quad of the lighting pass.
	/// </summary>
	wrl::ComPtr<ID3D11Buffer> m_lightingPassQuadVSBuffer;

	// Lights in the scene.
	sm::Vector3 m_lightingScales;	// Weight/scale for ambient, diffuse and specular term.
	bool usePointLights = false;
	float m_shininessExp;
	struct Light {
		sm::Vector4 Position;
		sm::Vector4 Color;
		sm::Vector4 Scale;
	};
	unsigned int m_seedValue = 42;
	unsigned int m_NR_LIGHTS = 32;
	std::vector<Light> m_lights;
	wrl::ComPtr<ID3D11Buffer> m_lightPosColorBuffer;
	wrl::ComPtr < ID3D11BlendState> m_additiveBlendState;
	wrl::ComPtr<ID3D11RasterizerState> m_rasterizerStateLightVolumes;

	// SSAO samplers.
	wrl::ComPtr<ID3D11SamplerState> m_gBufferSampler;
	wrl::ComPtr<ID3D11SamplerState> m_tiledTextureSampler;

	// SSAO occlusion map.
	wrl::ComPtr<ID3D11Texture2D> m_occlusionTexture;
	wrl::ComPtr<ID3D11RenderTargetView> m_occlusionTextureRTV;
	wrl::ComPtr < ID3D11ShaderResourceView> m_occlusionTextureSRV;

	// SSAO occlusion map (blurred).
	wrl::ComPtr<ID3D11Texture2D> m_occlusionTextureBlur;
	wrl::ComPtr<ID3D11RenderTargetView> m_occlusionTextureBlurRTV;
	wrl::ComPtr < ID3D11ShaderResourceView> m_occlusionTextureBlurSRV;

	// SSAO random vector noise texture.
	wrl::ComPtr<ID3D11Texture2D> m_randomVectorTexture;	//4x4
	wrl::ComPtr < ID3D11ShaderResourceView> m_randomVectorTextureSRV;

	// SSAO hemishphere sample kernel.
	struct SamplePosition {
		sm::Vector4 Position;
	};
	wrl::ComPtr<ID3D11Buffer> m_hemisphereKernelBuffer;

	// Window-filling quad for SSAO computations.
	sm::Matrix m_windowQuadModelMat;
	sm::Matrix m_windowQuadProjMat;
	std::shared_ptr <ModelClass> m_ssaoQuad;
	wrl::ComPtr<ID3D11Buffer> m_ssaoMPBuffer;

	// SSAO settings.
	struct SSAO_PARAMETER_BUFFER {
		float radius;			// Size of sphere.
		float bias;
		int kernelSize;	// How many samples we take.
		BOOL useSSAO;
	};
	wrl::ComPtr<ID3D11Buffer> m_ssaoParameterBuffer;
	float m_ssaoBias = 0.025;
	int m_ssaoKernelSize = 64;
	float m_ssaoRadius = 3.0;
	bool m_ssaoUseBlur = false;
	bool m_useSSAO = true;
	std::shared_ptr <ModelClass> m_ssaoQuadBlur;

	// G-Buffer textures and corresponding views. World pos, Diffuse Albedo
	// + Specular and normals.
	std::array<wrl::ComPtr<ID3D11Texture2D>, 2> m_gBufferTextures;
	std::array<wrl::ComPtr<ID3D11RenderTargetView>, 2> m_gBufferRTVs;
	std::array<wrl::ComPtr<ID3D11ShaderResourceView>, 2> m_gBufferSRVs;
	wrl::ComPtr<ID3D11Texture2D> m_gBufferDepthTexture;
	wrl::ComPtr<ID3D11DepthStencilView> m_gBufferDepthStencilView;
	wrl::ComPtr<ID3D11ShaderResourceView> m_gBufferDepthSRV;

	// Lighting pass textures (diffuse and specular lighting).
	std::array<wrl::ComPtr<ID3D11Texture2D>, 2> m_lightingTextures;
	std::array<wrl::ComPtr<ID3D11RenderTargetView>, 2> m_lightingTextureRTVs;
	std::array<wrl::ComPtr<ID3D11ShaderResourceView>, 2> m_lightingTextureSRVs;

	// Profiling queries. TODO: Move into profiler class. Dictionary.
	const unsigned int QUERY_BUFFER_CNT = 10;
	unsigned int m_currentQueryIdx = 0;
	// Basic queries.
	std::array<wrl::ComPtr <ID3D11Query>, 10> pQueryDisjoint;
	std::array<wrl::ComPtr <ID3D11Query>, 10> pQueryBeginFrame;
	std::array<wrl::ComPtr <ID3D11Query>, 10> pQueryEndFrame;

	// Other queries.
	std::array<wrl::ComPtr <ID3D11Query>, 10> pQueryLightViewPass;
	std::array<wrl::ComPtr <ID3D11Query>, 10> pGeometryPass;
	std::array<wrl::ComPtr <ID3D11Query>, 10> pLightVolumePass;
	std::array<wrl::ComPtr <ID3D11Query>, 10> pSSAO;
	std::array<wrl::ComPtr <ID3D11Query>, 10> pCombinationPass;
	std::array<wrl::ComPtr <ID3D11Query>, 10> pForwardPass;

	// Timings in Milliseconds. TODO: Move into profiler class. Dictionary.
	float m_msLightViewPass = 0.0;
	float m_msGeometryPass = 0.0;
	float m_msLightVolumePass = 0.0;
	float m_msSSAO = 0.0;
	float m_msCombinationPass = 0.0;
	float m_msForwardPass = 0.0;
	float m_msFrameTime = 0.0;
};
