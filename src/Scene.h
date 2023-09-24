#pragma once
#include "Helper.h"
#include "ModelClass.h"

/// <summary>
/// Base class for different types of scenes. All scenes will inherit this class.
/// </summary>
class Scene {
public:
	/// <summary>
	/// Delete default constructor.
	/// </summary>
	Scene() = delete;

	/// <summary>
	/// Constructor.
	/// </summary>
	/// <param name="sceneName">Name of the scene.</param>
	/// <param name="d3dDevice">D3D11 device in use.</param>
	/// <param name="d3dContext">D3D11 context in use.</param>
	Scene(
		std::string sceneName,
		wrl::ComPtr<ID3D11Device> d3dDevice,
		wrl::ComPtr<ID3D11DeviceContext> d3dContext,
		wrl::ComPtr<ID3D11RenderTargetView> d3dFrameBufferView,
		wrl::ComPtr<ID3D11DepthStencilView> d3dFrameBufferDepthStencilView,
		wrl::ComPtr<ID3D11DepthStencilState> d3dFrameBufferDepthState,
		D3D11_VIEWPORT viewport,
		std::shared_ptr <InputControls> controls);

	/// <summary>
	/// Inits all scene related structures. For greater flexibility in when and how
	/// a scene should be initialized.
	/// </summary>
	virtual void Init() = 0;

	/// <summary>
	/// Performs necessary rendering calls for drawing the scene. 
	/// </summary>
	virtual void Render(
		wrl::ComPtr<ID3D11RenderTargetView> d3dFrameBufferView,
		wrl::ComPtr<ID3D11DepthStencilView>d3dFrameBufferDepthStencilView) = 0;

	/// <summary>
	/// Returns name of the scene.
	/// </summary>
	/// <returns>Name of the scene as a std::string.</returns>
	std::string GetName();

	/// <summary>
	/// Tells the scene the current viewport resoltion.
	/// </summary>
	void NotifyResolution(D3D11_VIEWPORT viewport);

protected:
	/// <summary>
	/// Loads all models of the scene.
	/// </summary>
	virtual void initModels() = 0;

	/// <summary>
	/// Will be called so textures of the scene etc. can be resized.
	/// </summary>
	virtual void resizeEvent() = 0;

	// Direct3D resources.
	wrl::ComPtr<ID3D11Device>				m_d3dDevice;
	wrl::ComPtr<ID3D11DeviceContext>		m_d3dContext;
	wrl::ComPtr<ID3D11DepthStencilState>	m_d3dFrameBufferDepthState;

	// Other information.
	std::string m_sceneName;

	// Viewport information.
	D3D11_VIEWPORT m_viewport;
	int m_wWidth;
	int m_wHeight;
	float m_aspectRatio;
	sm::Vector4 m_pixelSize;

	// Controls.
	std::shared_ptr <InputControls> m_controls;
};

