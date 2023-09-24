#include "stdafx.h"
#include "Graphics.h"

/*
 * Graphics::Graphics
 */
Graphics::Graphics(HWND * hwnd, int wWidth, int wHeight,
        std::shared_ptr <InputControls> controls) : m_hwnd(nullptr),
        m_sceneIdx(0), m_clearColor(dx::XMVectorSet(0.0f, 0.0f, 1.0f,1.0f)){
    // Check if window handle is valid.
    assert(hwnd);
    m_hwnd = hwnd;
    m_wWidth = wWidth;
    m_wHeight = wHeight;
    m_controls = controls;
    m_useVsync = true;

    // Init all graphics related resources.
    initD3D11();

    // AFTER D3d11 is ready, create scenes and pass necessary resources.
    initScenes();

    // Init ImGui.
    initImGui();
}


/*
 * Graphics::changeScene
 */
void Graphics::changeScene(unsigned int index) {
    if (index >= this->m_sceneNames.size()) {
        throw std::invalid_argument("Selected scene index is invalid.");
    } else {
        // Reset current scene.
        m_Scene.reset();

        // Create selected scene. TODO: Make this more elegant.
        std::string selectedScene = m_sceneNames[index];
        if (selectedScene == "Sponza") {
            m_Scene = std::make_unique<SponzaScene>(
                "Sponza",
                m_d3dDevice,
                m_d3dContext,
                m_d3dFrameBufferView,
                m_d3dDepthStencilView,
                m_depthState,
                m_viewport,
                m_controls);
        }

        // Init the selected scene.
        m_Scene->Init();

        // Remember index.
        m_sceneIdx = index;

        // TODO: Add more sophisticated logging.
        m_log.push_back("Application: Loaded \"" + m_sceneNames[index] + "\".");
    }
}


/*
 * Graphics::initD3D11
 */
void Graphics::initD3D11(){
    // Get updated window information.
    RECT winRect;
    GetClientRect(*m_hwnd, &winRect);
    float wWidth = static_cast<FLOAT>(winRect.right - winRect.left);
    float wHeight = static_cast<FLOAT>(winRect.bottom - winRect.top);

    // Set viewport.
    m_viewport = {};
    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;
    m_viewport.Width = static_cast<FLOAT>(winRect.right - winRect.left);
    m_viewport.Height = static_cast<FLOAT>(winRect.bottom - winRect.top);
    m_viewport.MinDepth = 0;
    m_viewport.MaxDepth = 1;

    // Create device and swap-chain.
    DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 1;
    swapChainDesc.OutputWindow = *this->m_hwnd;
    swapChainDesc.Windowed = true;

    D3D_FEATURE_LEVEL featureLevel;
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;// TODO: Change to MT in future.
#if defined( DEBUG ) || defined( _DEBUG )
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT hr = D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0,
        flags, 0, 0, D3D11_SDK_VERSION, &swapChainDesc,
        m_d3dSwapChain.GetAddressOf(), m_d3dDevice.GetAddressOf(),
        &featureLevel, m_d3dContext.GetAddressOf());
    assert(S_OK == hr && m_d3dSwapChain && m_d3dDevice && m_d3dContext);

    // Access one of the swap-chain's back buffers.
    hr = m_d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &m_d3dFramebuffer);
    assert(SUCCEEDED(hr));

    // Create RenderTargetView for that back buffer. Will be used as the target
    // for the lighting pass.
    hr = m_d3dDevice->CreateRenderTargetView(
        m_d3dFramebuffer.Get(), 0, m_d3dFrameBufferView.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Depth texture.
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = wWidth;
    descDepth.Height = wHeight;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D32_FLOAT;   // TODO: Maybe change this.
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    hr = m_d3dDevice->CreateTexture2D(&descDepth, NULL,
        m_depthStencilTexture.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create depth stencil.
    D3D11_DEPTH_STENCIL_DESC stencilDesc = {};
    stencilDesc.DepthEnable = true;
    stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    stencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    stencilDesc.StencilEnable = false;

    // Create depth stencil state.
    hr = m_d3dDevice->CreateDepthStencilState(
        &stencilDesc, m_depthState.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create the depth stencil view.
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = DXGI_FORMAT_D32_FLOAT;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    descDSV.Flags = 0;
    hr = m_d3dDevice->CreateDepthStencilView(m_depthStencilTexture.Get(), &descDSV,
        m_d3dDepthStencilView.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create debug device for live object debugging.
    hr = m_d3dDevice->QueryInterface(__uuidof(ID3D11Debug), &m_d3dDebug);
    assert(SUCCEEDED(hr));

    // Enable alpha blending.
    // TODO: Render opaque mesh first. Then transparent mesh back to front.
    // --> Perform ordering of draw calls.
    //D3D11_BLEND_DESC blendDescription = {};
    //blendDescription.RenderTarget[0].BlendEnable = TRUE;
    //blendDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    //blendDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    //blendDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    //blendDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    //blendDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    //blendDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    //blendDescription.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    //hr = m_d3dDevice->CreateBlendState(&blendDescription, m_blendState.GetAddressOf());
    //float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    //UINT sampleMask = 0xffffffff;
    //m_d3dContext->OMSetBlendState(m_blendState.Get(), blendFactor, sampleMask);
}


/*
 * Graphics::initImGui
 */
void Graphics::initImGui() {
    // Setup Dear ImGui context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    m_imguiIO = ImGui::GetIO();
    m_imguiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style.
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends (D3D11 + Win32).
    ImGui_ImplWin32_Init(*m_hwnd);
    ImGui_ImplDX11_Init(m_d3dDevice.Get(), m_d3dContext.Get());
}


/*
 * Graphics::initScenes
 */
void Graphics::initScenes(){
    // TODO: Add other scenes to application.
    m_sceneNames.push_back("Sponza");

    // Use the first basic scene as standard scene.
    this->changeScene(m_sceneIdx);
}


/*
 * Graphics::renderGUI
 */
void Graphics::defineApplicationGUI(){
    // Track state changes.
    bool sceneChanged = false;

    // Start the Dear ImGui frame.
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Render main application GUI window.
    ImVec2 wSize = ImGui::GetMainViewport()->Size;
    ImVec2 mainMenuPos = ImVec2(0.75 * wSize[0], 0.01 * wSize[1]);
    ImVec2 mainMenuSize = ImVec2(0.245 * wSize[0], 0.3 * wSize[1]);
    ImGui::SetNextWindowPos(mainMenuPos, ImGuiCond_Appearing, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(mainMenuSize, ImGuiCond_Appearing);
    ImGui::Begin("D3D11 Project");

    // Scene selection. TODO: Create convenience function for ImGui::BeginCombo.
    std::string currentItem = m_sceneNames[m_sceneIdx];
    if (ImGui::BeginCombo("Scene", currentItem.c_str())) {
        for (int n = 0; n < m_sceneNames.size(); n++) {
            bool isSelected = (currentItem == m_sceneNames[n]);
            if (ImGui::Selectable(m_sceneNames[n].c_str(), isSelected)) {
                currentItem = m_sceneNames[n];
                changeScene(n);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Render basic settings.
    ImGui::Checkbox("Use V-Sync", &m_useVsync);

    // Render performance metrics.
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Performance Metrics:");
    // TODO: Use real frame times.
    const float my_values[] = { 0.2f, 0.1f, 1.0f, 0.5f, 0.9f, 2.2f };
    ImGui::PlotLines("Frame Times", my_values, IM_ARRAYSIZE(my_values));

    // Render log.
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Log:");
    ImGui::BeginChild("Scrolling");
    unsigned int logCounter = 0;
    for (const auto& msg : m_log) {
        ImGui::Text("%04d: %s", logCounter, msg.c_str());
        logCounter++;
    }
    ImGui::EndChild();

    // End of imgui definitions.
    ImGui::End();

    // Process state changes.
}


/*
 * Graphics::reportLiveObjects
 */
HRESULT Graphics::reportLiveObjects() {
    return m_d3dDebug->ReportLiveDeviceObjects(
        D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
}


/*
 * Graphics::RenderFrame
 */
void Graphics::RenderFrame(){    
    // Clear.
    m_d3dContext->ClearRenderTargetView(
        m_d3dFrameBufferView.Get(), dx::XMVECTOR(dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)).m128_f32);
    m_d3dContext->ClearDepthStencilView(m_d3dDepthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // Deactivate all shader stages to avoid any accidental usage of previously
    // bound shaders.
    m_d3dContext->VSSetShader(0, 0, 0); // Vertex shader.
    m_d3dContext->HSSetShader(0, 0, 0); // Hull shader.
                                        // Tesselation stage.
    m_d3dContext->DSSetShader(0, 0, 0); // Domain shader.
    m_d3dContext->GSSetShader(0, 0, 0); // Geometry shader.
    m_d3dContext->PSSetShader(0, 0, 0); // Pixel shader.

#if RENDER_GUI
    // Define the application GUI.
    defineApplicationGUI();
#endif

#if RENDER_SCENE
    // Render scene.
    m_Scene->Render(m_d3dFrameBufferView, m_d3dDepthStencilView);

#endif

#if RENDER_GUI
    // Debugger marks. TODO: smart
    ID3DUserDefinedAnnotation* pUDA = nullptr;
    m_d3dContext->QueryInterface(__uuidof(pUDA), (void**)&pUDA);
    pUDA->BeginEvent(L"ImGui");

    // Rendering of the Dear ImGui elements.
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    pUDA->EndEvent();
    pUDA->Release();
#endif

    // Present the frame (swap the buffers after ALL draw calls). Change to
    // Present(0,0) to disable vSync.
    m_d3dSwapChain->Present(m_useVsync, 0);
}


/*
 * Graphics::ResizeWindow
 */
void Graphics::ResizeWindow(int& width, int& height) {
    if (m_d3dSwapChain && width>0 && height>0) {
        m_d3dContext->OMSetRenderTargets(0, 0, 0);

        // Release all outstanding references to the swap chain's buffers.
        m_d3dFramebuffer.Reset();
        m_d3dFrameBufferView.Reset();
        m_depthStencilTexture.Reset();
        m_d3dDepthStencilView.Reset();

        // Preserve the existing buffer count and format.
        // Automatically choose the width and height to match the client rect.
        HRESULT hr = m_d3dSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        assert(SUCCEEDED(hr));

        DXGI_SWAP_CHAIN_DESC scDesc = {};
        hr = m_d3dSwapChain->GetDesc(&scDesc);
        assert(SUCCEEDED(hr));

        // Get an ID3D11Texture2D interface for the back buffer from swapchain.
        hr = m_d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
            &m_d3dFramebuffer);
        assert(SUCCEEDED(hr));

        // Create RenderTargetView for frame buffer.
        hr = m_d3dDevice->CreateRenderTargetView(m_d3dFramebuffer.Get(), 0,
            m_d3dFrameBufferView.GetAddressOf());
        assert(SUCCEEDED(hr));

        // Get updated framebuffer information.
        D3D11_TEXTURE2D_DESC desc;
        m_d3dFramebuffer->GetDesc(&desc);

        // Set window size in Application.
        width = desc.Width;
        height = desc.Height;

        // Set up the viewport.
        m_viewport = {};
        m_viewport.Width = desc.Width;
        m_viewport.Height = desc.Height;
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;
        m_viewport.TopLeftX = 0;
        m_viewport.TopLeftY = 0;
        m_d3dContext->RSSetViewports(1, &m_viewport);

        // Depth texture.
        D3D11_TEXTURE2D_DESC descDepth = {};
        descDepth.Width = desc.Width;
        descDepth.Height = desc.Height;
        descDepth.MipLevels = 1;
        descDepth.ArraySize = 1;
        descDepth.Format = DXGI_FORMAT_D32_FLOAT;   // TODO: Maybe change this.
        descDepth.SampleDesc.Count = 1;
        descDepth.SampleDesc.Quality = 0;
        descDepth.Usage = D3D11_USAGE_DEFAULT;
        descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        descDepth.CPUAccessFlags = 0;
        descDepth.MiscFlags = 0;
        hr = m_d3dDevice->CreateTexture2D(&descDepth, NULL,
            m_depthStencilTexture.GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create the depth stencil view.
        D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
        descDSV.Format = DXGI_FORMAT_D32_FLOAT;
        descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        descDSV.Flags = 0;
        hr = m_d3dDevice->CreateDepthStencilView(m_depthStencilTexture.Get(),
            &descDSV, m_d3dDepthStencilView.GetAddressOf());
        assert(SUCCEEDED(hr));

        // Set RenderTargets.
        m_d3dContext->OMSetRenderTargets(1, m_d3dFrameBufferView.GetAddressOf(),
            m_d3dDepthStencilView.Get());

        // TODO: Update ImGui window size (not working for some reason).
        m_imguiIO.DisplaySize = ImVec2(desc.Width, desc.Height);

        // Notify current scene.
        m_wWidth = width;
        m_wHeight = height;
        m_Scene->NotifyResolution(m_viewport);
    }
}
