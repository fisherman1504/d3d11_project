#pragma once

// ImGui.
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Helper.h"

// Scenes. TODO: Move into separate file.
#include "SponzaScene.h"

#define RENDER_GUI 1
#define RENDER_SCENE 1

/// <summary>
/// Implementation of all D3D11 related systems.
/// </summary>
class Graphics {
public:
    Graphics(HWND* hwnd, int wWidth, int wHeight,
        std::shared_ptr <InputControls> controls);

    /// <summary>
    /// Performs all D3D11 related calls to render a frame.
    /// </summary>
    void RenderFrame();

    /// <summary>
    /// Used when window gets resized (WM_SIZE). Called from Main.cpp.
    /// </summary>
    /// <remarks>
    /// Taken from https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/
    /// d3d10-graphics-programming-guide-dxgi#handling-window-resizing and
    /// https://gamedev.net/forums/topic/623652-how-should-i-resize-a-directx-11-
    /// window/4932665/  (Real MJP comment).
    /// <param name="width"></param>
    /// <param name="height"></param>
    void ResizeWindow(int& width, int& height);

    // TODO: Getters for device, context etc.

private:
    /// <summary>
    /// Switch the scene that is currently rendered.
    /// </summary>
    /// <param name="index">Index of the scene.</param>
    void changeScene(unsigned int index);

    /// <summary>
    /// Inits all Direct3D systems.
    /// </summary>
    void initD3D11();

    /// <summary>
    /// Inits ImGui for rendering UI.
    /// </summary>
    void initImGui();

    /// <summary>
    /// Inits scene structures.
    /// </summary>
    void initScenes();

    /// <summary>
    /// Defines the application 2D user interface via ImGui.
    /// </summary>
    void defineApplicationGUI();

    /// <summary>
    /// Returns reference counts for all used D3D11 objects.
    /// </summary>
    /// <remarks>
    /// Useful for debugging.
    /// </remarks>
    HRESULT reportLiveObjects();

    // Framebuffer.
    wrl::ComPtr<ID3D11Device> m_d3dDevice;
    wrl::ComPtr<ID3D11DeviceContext> m_d3dContext;
    wrl::ComPtr<IDXGISwapChain> m_d3dSwapChain;
    wrl::ComPtr<ID3D11Texture2D> m_d3dFramebuffer;
    wrl::ComPtr<ID3D11RenderTargetView> m_d3dFrameBufferView;
    wrl::ComPtr<ID3D11DepthStencilState> m_depthState;
    wrl::ComPtr<ID3D11Texture2D> m_depthStencilTexture;
    wrl::ComPtr<ID3D11DepthStencilView> m_d3dDepthStencilView;

    // Other stuff.
    wrl::ComPtr<ID3D11Debug> m_d3dDebug;
    dx::XMVECTOR m_clearColor;
    wrl::ComPtr<ID3D11BlendState> m_blendState;

    // Scenes.
    int m_sceneIdx;
    std::unique_ptr<Scene> m_Scene;
    std::vector<std::string> m_sceneNames;

    // Win32 systems.
    HWND* m_hwnd; // window handle.
    int m_wWidth;
    int m_wHeight;
    D3D11_VIEWPORT m_viewport;

    // ImGui library.
    ImGuiIO m_imguiIO;
    std::vector<std::string> m_log;

    // GUI elements.
    bool m_useVsync;

    // Controls.
    std::shared_ptr <InputControls> m_controls;
};

