#include "stdafx.h"
#include "Scene.h"


/*
 * Scene::GetName
 */
std::string Scene::GetName() {
    return this->m_sceneName;
}


/*
 * Scene::NotifyResolution
 */
void Scene::NotifyResolution(D3D11_VIEWPORT viewport) {
    // Store updated width and height of the viewport.
    m_viewport = viewport;

    // Store separetly for more convenient access.
    m_wWidth = viewport.Width;
    m_wHeight = viewport.Height;

    // Compute updated aspect ratio.
    if (m_wWidth > 0 && m_wHeight > 0) {
        m_aspectRatio = static_cast<float>(m_wWidth) /
            static_cast<float>(m_wHeight);
    } else {
        m_aspectRatio = 1.0;
    }

    // Update pixel size.
    m_pixelSize = sm::Vector4(1.0 / static_cast<float>(m_wWidth),
        1.0 / static_cast<float>(m_wHeight), 0.0, 0.0);

    // Make remaining updates to textures or buffers of the specific scene.
    resizeEvent();
}


/*
 * Scene::Scene
 */
Scene::Scene(
        std::string sceneName,
        wrl::ComPtr<ID3D11Device> d3dDevice,
        wrl::ComPtr<ID3D11DeviceContext> d3dContext,
        wrl::ComPtr<ID3D11RenderTargetView> d3dFrameBufferView,
        wrl::ComPtr<ID3D11DepthStencilView> d3dDepthStencilView,
        wrl::ComPtr<ID3D11DepthStencilState> depthState,
        D3D11_VIEWPORT viewport,
        std::shared_ptr <InputControls> controls) {
    // Store basic information.
    m_sceneName = sceneName;
    m_controls = controls;
    m_viewport = viewport;
    m_wWidth = m_viewport.Width;
    m_wHeight = m_viewport.Height;
    if (m_wWidth > 0 && m_wHeight > 0) {
        m_aspectRatio = static_cast<float>(m_wWidth) /
            static_cast<float>(m_wHeight);
    } else {
        m_aspectRatio = 1.0;
    }
    m_pixelSize = sm::Vector4(1.0 / static_cast<float>(m_wWidth),
        1.0 / static_cast<float>(m_wHeight), 0.0, 0.0);
    
    // Usual D3D11 resources.
    m_d3dDevice = d3dDevice;
    m_d3dContext = d3dContext;
    m_d3dFrameBufferDepthState = depthState;
}
