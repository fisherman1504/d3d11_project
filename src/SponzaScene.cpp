#include "stdafx.h"
#include "SponzaScene.h"


/*
 * SponzaScene::createTimestampQuery
 */
wrl::ComPtr <ID3D11Query> SponzaScene::createTimestampQuery(
        wrl::ComPtr<ID3D11Device> device) {
    D3D11_QUERY_DESC queryDesc;
    queryDesc.Query = D3D11_QUERY_TIMESTAMP;
    queryDesc.MiscFlags = 0;

    wrl::ComPtr <ID3D11Query> query;
    device->CreateQuery(&queryDesc, query.GetAddressOf());
    return query;
}


/*
 * SponzaScene::createDisjointQuery
 */
wrl::ComPtr <ID3D11Query> SponzaScene::createDisjointQuery(
        wrl::ComPtr<ID3D11Device> device) {
    D3D11_QUERY_DESC queryDesc;
    queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    queryDesc.MiscFlags = 0;

    wrl::ComPtr <ID3D11Query> query;
    device->CreateQuery(&queryDesc, query.GetAddressOf());
    return query;
}


/*
 * SponzaScene::Render
 */
void SponzaScene::Render(wrl::ComPtr<ID3D11RenderTargetView>		d3dFrameBufferView,
    wrl::ComPtr<ID3D11DepthStencilView>		d3dFrameBufferDepthStencilView) {
    // Update matrices, buffers etc.
    update();

    ID3D11ShaderResourceView* nullSRV[10] = { nullptr };
    ID3D11Buffer* nullBuffers[10] = { nullptr };

    // Debugger marks. TODO: smart
    ID3DUserDefinedAnnotation * pUDA = nullptr;
    m_d3dContext->QueryInterface(__uuidof(pUDA), (void**)&pUDA);

    // Begin disjoint query, and timestamp the beginning of the frame
    m_d3dContext->Begin(pQueryDisjoint[m_currentQueryIdx].Get());
    m_d3dContext->End(pQueryBeginFrame[m_currentQueryIdx].Get());

    //##############################################################################
    // Light view pass (directional light shadow mapping).
    pUDA->BeginEvent(L"Directional Light View Pass");

    if (m_useShadows) {
        m_d3dContext->ClearDepthStencilView(m_shadowDepthView.Get(),
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // Set rendering state and viewport. Use front face culling to reduce
        // peter panning. TODO: Only apply to objects where it makes sense:
        // https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
        m_d3dContext->RSSetViewports(1, &m_shadowViewport);
        m_d3dContext->RSSetState(m_rasterizerStateShadows.Get());
        m_d3dContext->OMSetDepthStencilState(m_shadowDepthStencilState.Get(), 0);
        m_d3dContext->OMSetRenderTargets(0, nullptr, m_shadowDepthView.Get());

        // Pass light view and projection matrix. First slot is always reserved 
        // for ModelClass information.
        m_d3dContext->VSSetConstantBuffers(1, 1,
            m_shadowConstBufferVS.GetAddressOf());

        // Draw all models that can cause shadows.
        m_sponzaModel->Draw(true);

        // Cleanup.
        m_d3dContext->OMSetDepthStencilState(nullptr, 0);
        m_d3dContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // Restore the default blend state
        m_d3dContext->VSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->PSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->VSSetConstantBuffers(0, 10, nullBuffers);
        m_d3dContext->PSSetConstantBuffers(0, 10, nullBuffers);

    }
    m_d3dContext->End(pQueryLightViewPass[m_currentQueryIdx].Get());
    pUDA->EndEvent();

    //##############################################################################
    // Deferred: G-Pass.
    pUDA->BeginEvent(L"Deferred: G-Pass");
    {
        // Clear g-buffer every frame.
        for (const auto& ptr : m_gBufferRTVs) {
            m_d3dContext->ClearRenderTargetView(ptr.Get(),
                dx::XMVECTOR(dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)).m128_f32);
        }

        m_d3dContext->ClearDepthStencilView(m_gBufferDepthStencilView.Get(),
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // Set every frame.
        m_d3dContext->RSSetViewports(1, &m_viewport);
        m_d3dContext->RSSetState(m_rasterizerState.Get());
        m_d3dContext->OMSetDepthStencilState(m_d3dFrameBufferDepthState.Get(), 1);
        m_d3dContext->OMSetRenderTargets(m_gBufferRTVs.size(),
            m_gBufferRTVs.data()->GetAddressOf(), m_gBufferDepthStencilView.Get());

        // First slot is always reserved for ModelClass information.
        m_d3dContext->VSSetConstantBuffers(1, 1, m_sceneBufferVS.GetAddressOf());

        // Draw the sponza scene.
        m_sponzaModel->Draw(false);

        // Cleanup.
        m_d3dContext->OMSetDepthStencilState(nullptr, 0);
        m_d3dContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // Restore the default blend state
        m_d3dContext->VSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->PSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->VSSetConstantBuffers(0, 10, nullBuffers);
        m_d3dContext->PSSetConstantBuffers(0, 10, nullBuffers);
    }
    m_d3dContext->End(pGeometryPass[m_currentQueryIdx].Get());
    pUDA->EndEvent();

    //##############################################################################
    // Deferred: Lighting pass
    pUDA->BeginEvent(L"Deferred: Lighting Pass");
    {
        // Clear all render targets.
        for (const auto& ptr : m_lightingTextureRTVs) {
            m_d3dContext->ClearRenderTargetView(ptr.Get(),
                dx::XMVECTOR(dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)).m128_f32);
        }

        m_d3dContext->RSSetViewports(1, &m_viewport);
        m_d3dContext->OMSetDepthStencilState(m_d3dFrameBufferDepthState.Get(), 1);

        // Switch to front culling for the case that we are inside a volume.
        m_d3dContext->RSSetState(m_rasterizerStateLightVolumes.Get());

        // We do additive blending.
        m_d3dContext->OMSetBlendState(m_additiveBlendState.Get(), nullptr, 0xFFFFFFFF);

        // Bind G-Buffer sampler.
        m_d3dContext->PSSetSamplers(1, 1, m_gBufferSampler.GetAddressOf());

        // We dont need a depth texture.
        m_d3dContext->OMSetRenderTargets(m_lightingTextureRTVs.size(),
            m_lightingTextureRTVs.data()->GetAddressOf(),0);

        // Bind buffers.
        m_d3dContext->VSSetConstantBuffers(1, 1, m_sceneBufferVS.GetAddressOf());
        m_d3dContext->PSSetConstantBuffers(1, 1, m_sceneBufferPS.GetAddressOf());

        // Bind G-Buffer.
        m_d3dContext->PSSetShaderResources(0, 1, m_gBufferDepthSRV.GetAddressOf());
        m_d3dContext->PSSetShaderResources(1, 1, m_gBufferSRVs[0].GetAddressOf());

        // Draw light volumes.
        if (usePointLights) {
            m_lightVolumes->Draw(false);
        }

        // Cleanup.
        m_d3dContext->OMSetDepthStencilState(nullptr, 0);
        m_d3dContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // Restore the default blend state
        m_d3dContext->VSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->PSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->VSSetConstantBuffers(0, 10, nullBuffers);
        m_d3dContext->PSSetConstantBuffers(0, 10, nullBuffers);
    }
    m_d3dContext->End(pLightVolumePass[m_currentQueryIdx].Get());
    pUDA->EndEvent();

    //##############################################################################
    // SSAO.
    pUDA->BeginEvent(L"SSAO: Occlusion Map");

    if (m_useSSAO) {
        // Set every frame.
        m_d3dContext->RSSetState(m_rasterizerState.Get());
        m_d3dContext->ClearRenderTargetView(m_occlusionTextureRTV.Get(),
            dx::XMVECTOR(dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)).m128_f32);
        m_d3dContext->RSSetViewports(1, &m_viewport);
        m_d3dContext->OMSetDepthStencilState(m_d3dFrameBufferDepthState.Get(), 1);

        // We dont need a depth texture.
        m_d3dContext->OMSetRenderTargets(1, m_occlusionTextureRTV.GetAddressOf(), 0);

        // Bind special SSAO sampler.
        m_d3dContext->PSSetSamplers(1, 1, m_gBufferSampler.GetAddressOf());
        m_d3dContext->PSSetSamplers(2, 1, m_tiledTextureSampler.GetAddressOf());

        // Bind buffers.
        m_d3dContext->VSSetConstantBuffers(1, 1, m_ssaoMPBuffer.GetAddressOf());            // Quad model and proj matrix.
        m_d3dContext->PSSetConstantBuffers(1, 1, m_sceneBufferPS.GetAddressOf());           // Matrices, camera position, ...
        m_d3dContext->PSSetConstantBuffers(2, 1, m_hemisphereKernelBuffer.GetAddressOf());  // Sample positions.
        m_d3dContext->PSSetConstantBuffers(3, 1, m_ssaoParameterBuffer.GetAddressOf());  // SSAO parameters.

        // Bind textures.
         m_d3dContext->PSSetShaderResources(0, 1, m_gBufferDepthSRV.GetAddressOf());        // gBuffer depth.
         m_d3dContext->PSSetShaderResources(1, 1, m_gBufferSRVs[0].GetAddressOf());
         m_d3dContext->PSSetShaderResources(2, 1, m_randomVectorTextureSRV.GetAddressOf()); // 4x4 random vector noise texture.

        // Compute occlusion map.
         m_ssaoQuad->Draw(false);

         // Cleanup.
         m_d3dContext->OMSetDepthStencilState(nullptr, 0);
         m_d3dContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // Restore the default blend state
         m_d3dContext->VSSetShaderResources(0, 10, nullSRV);
         m_d3dContext->PSSetShaderResources(0, 10, nullSRV);
         m_d3dContext->VSSetConstantBuffers(0, 10, nullBuffers);
         m_d3dContext->PSSetConstantBuffers(0, 10, nullBuffers);
    }
    pUDA->EndEvent();

    // Blur the computed occlusion map.
    pUDA->BeginEvent(L"SSAO: Blur Application");
    if(m_useSSAO && m_ssaoUseBlur){
        // Set every frame.
        m_d3dContext->RSSetState(m_rasterizerState.Get());
        m_d3dContext->ClearRenderTargetView(m_occlusionTextureBlurRTV.Get(),
            dx::XMVECTOR(dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)).m128_f32);
        m_d3dContext->RSSetViewports(1, &m_viewport);
        m_d3dContext->OMSetDepthStencilState(m_d3dFrameBufferDepthState.Get(), 1);

        // We dont need a depth texture. Target will be the blurred occlusion map.
        m_d3dContext->OMSetRenderTargets(1, m_occlusionTextureBlurRTV.GetAddressOf(), 0);

        // Bind special SSAO sampler.
        m_d3dContext->PSSetSamplers(1, 1, m_gBufferSampler.GetAddressOf());

        // Bind buffers.
        m_d3dContext->VSSetConstantBuffers(1, 1, m_ssaoMPBuffer.GetAddressOf());            // Quad model and proj matrix.
        m_d3dContext->PSSetConstantBuffers(1, 1, m_sceneBufferPS.GetAddressOf());           // Matrices, camera position, ...

        // Bind the unblurred occlusion texture.
        m_d3dContext->PSSetShaderResources(0, 1, m_occlusionTextureSRV.GetAddressOf());

       // Draw window-filling quad and perform blur.
        m_ssaoQuadBlur->Draw(false);

        // Cleanup.
        m_d3dContext->OMSetDepthStencilState(nullptr, 0);
        m_d3dContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // Restore the default blend state
        m_d3dContext->VSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->PSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->VSSetConstantBuffers(0, 10, nullBuffers);
        m_d3dContext->PSSetConstantBuffers(0, 10, nullBuffers);
    }

    m_d3dContext->End(pSSAO[m_currentQueryIdx].Get());
    pUDA->EndEvent();

    //##############################################################################
    // Deferred: Combination pass.
    pUDA->BeginEvent(L"Deferred: Combination Pass.");
    {
        // Clearing of the framebuffer is done outside in Graphics::Render.
        m_d3dContext->RSSetViewports(1, &m_viewport);
        m_d3dContext->RSSetState(m_rasterizerState.Get()); // Set every frame.
        m_d3dContext->OMSetDepthStencilState(m_d3dFrameBufferDepthState.Get(), 1);
        m_d3dContext->OMSetRenderTargets(1, d3dFrameBufferView.GetAddressOf(),
            d3dFrameBufferDepthStencilView.Get());

        // First slot is always reserved for ModelClass information.
        m_d3dContext->VSSetConstantBuffers(1, 1, m_sceneBufferVS.GetAddressOf());
        m_d3dContext->VSSetConstantBuffers(2, 1, m_lightingPassQuadVSBuffer.GetAddressOf());
        
        m_d3dContext->PSSetConstantBuffers(1, 1, m_sceneBufferPS.GetAddressOf());
        m_d3dContext->PSSetConstantBuffers(2, 1, m_shadowConstBufferPS.GetAddressOf());
        m_d3dContext->PSSetConstantBuffers(3, 1, m_ssaoParameterBuffer.GetAddressOf());  // SSAO parameters.

        // Bind light camera depth.
        m_d3dContext->PSSetShaderResources(0, 1, m_shadowResourceView.GetAddressOf());
        m_d3dContext->PSSetSamplers(1, 1, m_comparisonSampler_point.GetAddressOf());
        m_d3dContext->PSSetSamplers(2, 1, m_gBufferSampler.GetAddressOf());

        // Bind albedo diffuse and normal texture from gBuffer.
        m_d3dContext->PSSetShaderResources(1, 1, m_gBufferDepthSRV.GetAddressOf());
        m_d3dContext->PSSetShaderResources(2, 1, m_gBufferSRVs[0].GetAddressOf());
        m_d3dContext->PSSetShaderResources(3, 1, m_gBufferSRVs[1].GetAddressOf());

        // Bind the lighting textures of the point lights. 
        m_d3dContext->PSSetShaderResources(4, 1, m_lightingTextureSRVs[0].GetAddressOf());
        m_d3dContext->PSSetShaderResources(5, 1, m_lightingTextureSRVs[1].GetAddressOf());

        // Use the original or blurred occlusion map.
        if (m_ssaoUseBlur) {
            m_d3dContext->PSSetShaderResources(6, 1, m_occlusionTextureBlurSRV.GetAddressOf());
        } else {
            m_d3dContext->PSSetShaderResources(6, 1, m_occlusionTextureSRV.GetAddressOf());
        }

        // Draw the quad.
        m_lightingPassQuad->Draw(false);

        // Cleanup.
        m_d3dContext->OMSetDepthStencilState(nullptr, 0);
        m_d3dContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // Restore the default blend state
        m_d3dContext->VSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->PSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->VSSetConstantBuffers(0, 10, nullBuffers);
        m_d3dContext->PSSetConstantBuffers(0, 10, nullBuffers);
    }

    m_d3dContext->End(pCombinationPass[m_currentQueryIdx].Get());
    pUDA->EndEvent();

    //##############################################################################
    // Forward pass after the lighting pass.
    pUDA->BeginEvent(L"Forward Pass");
    {
        m_d3dContext->RSSetViewports(1, &m_viewport);
        m_d3dContext->RSSetState(m_rasterizerState.Get()); // Set every frame.
        m_d3dContext->OMSetDepthStencilState(m_d3dFrameBufferDepthState.Get(), 1);

        // Use depth stencil view from gBuffer!!!!!!!!!!
        m_d3dContext->OMSetRenderTargets(1, d3dFrameBufferView.GetAddressOf(),
            m_gBufferDepthStencilView.Get());

        // Draw the skybox.
        if (m_useSkyBox) {
            // Bind buffers and textures.
            m_d3dContext->VSSetConstantBuffers(1, 1, m_sceneBufferVS.GetAddressOf());
            m_d3dContext->PSSetConstantBuffers(1, 1, m_sceneBufferPS.GetAddressOf());
            m_d3dContext->PSSetShaderResources(0, 1, m_skyBoxTexture.srv.GetAddressOf());

            // Draw the skybox cube using the cube map.
            m_skyBoxCube->Draw(false);

            // Cleanup.
            m_d3dContext->OMSetDepthStencilState(nullptr, 0);
            m_d3dContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // Restore the default blend state
            m_d3dContext->VSSetShaderResources(0, 10, nullSRV);
            m_d3dContext->PSSetShaderResources(0, 10, nullSRV);
            m_d3dContext->VSSetConstantBuffers(0, 10, nullBuffers);
            m_d3dContext->PSSetConstantBuffers(0, 10, nullBuffers);
        }

        // Draw visualizations.
        {
            // First slot is always reserved for Mesh material information.
            m_d3dContext->VSSetConstantBuffers(1, 1, m_sceneBufferVS.GetAddressOf());
            m_d3dContext->PSSetConstantBuffers(1, 1, m_sceneBufferPS.GetAddressOf());

            // Draw origin visualization.
            if (m_showOriginVis) {
                m_originVisualization->Draw(false);
            }

            // Draw point light visualization.
            if (usePointLights) {
                m_pointLightVisualization->Draw(false);
            }

            // Cleanup.
            m_d3dContext->OMSetDepthStencilState(nullptr, 0);
            m_d3dContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // Restore the default blend state
            m_d3dContext->VSSetShaderResources(0, 10, nullSRV);
            m_d3dContext->PSSetShaderResources(0, 10, nullSRV);
            m_d3dContext->VSSetConstantBuffers(0, 10, nullBuffers);
            m_d3dContext->PSSetConstantBuffers(0, 10, nullBuffers);
        }
    }

    //##############################################################################
    // Render things on top of everything in the framebuffer (GUI, ...).
    
    // Render texture visualization quad.
    if (m_showTexVis) {
        m_d3dContext->RSSetViewports(1, &m_viewport);
        m_d3dContext->OMSetDepthStencilState(m_d3dFrameBufferDepthState.Get(), 1);
        m_d3dContext->RSSetState(m_rasterizerState.Get());
        m_d3dContext->OMSetRenderTargets(1, d3dFrameBufferView.GetAddressOf(),
            d3dFrameBufferDepthStencilView.Get());

        // Set required textures and buffers for visualizion.
        m_d3dContext->VSSetConstantBuffers(2, 1, m_texVisConstBuffer.GetAddressOf());
        m_d3dContext->PSSetConstantBuffers(1, 1, m_texVisPSBuffer.GetAddressOf());
       
        m_d3dContext->PSSetShaderResources(0, 1, m_gBufferDepthSRV.GetAddressOf());
        m_d3dContext->PSSetShaderResources(1, 1, m_shadowResourceView.GetAddressOf());
        m_d3dContext->PSSetShaderResources(2, 1, m_lightingTextureSRVs[0].GetAddressOf());
        m_d3dContext->PSSetShaderResources(3, 1, m_lightingTextureSRVs[1].GetAddressOf());
        m_d3dContext->PSSetShaderResources(4, 1, m_occlusionTextureSRV.GetAddressOf());
        m_d3dContext->PSSetShaderResources(5, 1, m_occlusionTextureBlurSRV.GetAddressOf());

        // Set samplers.
        m_d3dContext->PSSetSamplers(1, 1, m_gBufferSampler.GetAddressOf());


        // Draw texture visualization quad.
        m_texVisQuad->Draw(false);

        // Cleanup.
        m_d3dContext->OMSetDepthStencilState(nullptr, 0);
        m_d3dContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // Restore the default blend state
        m_d3dContext->VSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->PSSetShaderResources(0, 10, nullSRV);
        m_d3dContext->VSSetConstantBuffers(0, 10, nullBuffers);
        m_d3dContext->PSSetConstantBuffers(0, 10, nullBuffers);
    }

    m_d3dContext->End(pForwardPass[m_currentQueryIdx].Get());
    pUDA->EndEvent();

    pUDA->Release();

    // Finish up queries
    m_d3dContext->End(pQueryEndFrame[m_currentQueryIdx].Get());
    m_d3dContext->End(pQueryDisjoint[m_currentQueryIdx].Get());

    // Process queries.
    processPerformanceMetrics();

    // Define GUI.
    this->defineImGui();
}


/*
 * SponzaScene::Init
 */
void SponzaScene::Init() {
    // GUI related variables.
    m_showWireframe = false;
    useAnimation = false;
    m_drawMode = 0;

    // Lighting setup.
    m_lightingScales = { 0.3, 1.0, 1.0 };
    m_shininessExp = 20.0f;

    // Get orientation from model.
    float m_modelYaw = 0.0f;
    float m_modelPitch = 0.0f;
    float m_modelRoll = 0.0f;

    // Setup camera.
    m_cameraPitch = 0.100;
    m_cameraYaw = -1.563;
    m_viewPos = { 70.413,4.255,1.095 };
    m_projMat = sm::Matrix::CreatePerspectiveFieldOfView(dx::XM_PI / 4.0f,
        float(m_wWidth) / float(m_wHeight), m_cameraNearClip, m_cameraFarClip);
    m_viewMat = sm::Matrix::Identity;

    // Init rasterizer settings. TODO: Add anti aliasing support.
    m_rasterDesc.FillMode = m_showWireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
    m_rasterDesc.CullMode = D3D11_CULL_BACK;
    m_rasterDesc.FrontCounterClockwise = true;
    m_rasterDesc.DepthBias = 0;
    m_rasterDesc.DepthBiasClamp = 0.0f;
    m_rasterDesc.SlopeScaledDepthBias = 0.0f;
    m_rasterDesc.DepthClipEnable = true;
    m_rasterDesc.ScissorEnable = false;
    m_rasterDesc.MultisampleEnable = false;
    m_rasterDesc.AntialiasedLineEnable = false;
    HRESULT hr = m_d3dDevice->CreateRasterizerState(
        &m_rasterDesc, m_rasterizerState.GetAddressOf());

    // Init the G-Buffer for deferred rendering.
    initGBuffer();

    // Init lights in the scene.
    initLights();

    // Init models of the scene.
    initModels();

    // Init all scene related buffers.
    initSceneBuffers();

    // Init resources for texture visualization quad.
    initTextureVisualization();

    // Init shadow mapping via directional light.
    initShadows();

    // Create cube and load cube map from disk for skybox.
    initSkyBox();

    // Init window-filling quad for lighting pass.
    initLightingPass();

    // Init textures and buffers for SSAO.
    initSSAO();

    // Init queries for profiling.
    initProfiling();
}


/*
 * SponzaScene::initModels
 */
void SponzaScene::initModels() {
    // Create sponza scene.
    std::string modelName = "sponza";
    std::string modelPath = Helper::GetAssetFullPathString(
        "\\Assets\\Models\\" + modelName + "\\");
    m_sponzaModel = std::make_shared<ModelClass>(modelPath, modelName,
        (aiPostProcessSteps::aiProcess_FlipUVs | 
            aiPostProcessSteps::aiProcess_CalcTangentSpace),
        m_d3dDevice, m_d3dContext,
        &m_viewMat, 1, sm::Vector3{ 0.0, -10.0, 0.0 },
        sm::Vector4{ 0.0, 0.0, 0.0, 1.0 }, 1, L"\\src\\shader\\Sponza_vs.hlsl",
        L"\\src\\shader\\Sponza_ps.hlsl");

    // Create world origin visualization cube.
    m_originVisualization = std::make_shared<ModelClass>(ModelClass::BaseType::CUBE,
        sm::Vector3(1.0,0.0,0.0), m_d3dDevice, m_d3dContext,
        &m_viewMat, 1, sm::Vector3{ 0.0, 0.0, 0.0 },
        sm::Vector4{ 0.0, 0.0, 0.0, 1.0 }, L"\\src\\shader\\Basic_vs.hlsl",
        L"\\src\\shader\\Basic_ps.hlsl");

    // Create light visualization.
    std::vector<sm::Vector3> positions;
    std::vector<sm::Vector3> colors;
    std::vector<sm::Vector3> scales;
    std::vector <sm::Vector4> rotations;
    assert(m_lights.size());
    for (const Light& light : m_lights) {
        positions.push_back(sm::Vector3(light.Position.x, light.Position.y, light.Position.z));
        colors.push_back(sm::Vector3(light.Color.x, light.Color.y, light.Color.z));
        scales.push_back(sm::Vector3(light.Scale.x, light.Scale.y, light.Scale.z));
        rotations.push_back(sm::Vector4(0.0, 0.0, 0.0, 1.0));
    }
    m_pointLightVisualization = std::make_shared<ModelClass>(
        ModelClass::BaseType::SPHERE,
        positions,
        colors,
        scales,
        rotations,
        m_d3dDevice,
        m_d3dContext,
        &m_viewMat,
        L"\\src\\shader\\BasicInstanced_vs.hlsl",
        L"\\src\\shader\\BasicInstanced_ps.hlsl");

    // Use the data and a custom shader for light volume instanced rendering.
    m_lightVolumes = std::make_shared<ModelClass>(
        ModelClass::BaseType::SPHERE,
        positions,
        colors,
        scales,
        rotations,
        m_d3dDevice,
        m_d3dContext,
        &m_viewMat,
        L"\\src\\shader\\LightVolumeInstanced_vs.hlsl",
        L"\\src\\shader\\LightVolumeInstanced_ps.hlsl");
}


/*
 * SponzaScene::resizeEvent
 */
void SponzaScene::resizeEvent() {
    // Release existing G-buffer resources.
    for (unsigned int i = 0; i < m_gBufferTextures.size();i++) {
        m_gBufferTextures[i].Reset();
        m_gBufferRTVs[i].Reset();
        m_gBufferSRVs[i].Reset();
    }
    m_gBufferDepthTexture.Reset();
    m_gBufferDepthStencilView.Reset();
    m_gBufferDepthSRV.Reset();;

    // Release lighting pass textures (diffuse and specular lighting).
    for (unsigned int i = 0; i < m_lightingTextures.size(); i++) {
        m_lightingTextures[i].Reset();
        m_lightingTextureRTVs[i].Reset();
        m_lightingTextureSRVs[i].Reset();
    }

    // Create new textures for G-Buffer and lighting pass textures.
    initGBuffer();

    // Release SSAO occlusion maps.
    m_occlusionTexture.Reset();
    m_occlusionTextureRTV.Reset();
    m_occlusionTextureSRV.Reset();
    m_occlusionTextureBlur.Reset();
    m_occlusionTextureBlurRTV.Reset();
    m_occlusionTextureBlurSRV.Reset();

    // Create new SSAO occlusion maps.
    initSSAOOccMaps();
}


/*
 * SponzaScene::defineImGui
 */
void SponzaScene::defineImGui() {
    // Track state changes.
    bool modelStateChanged = false;
    bool modelChanged = false;
    bool cameraChanged = false;
    bool rasterizerChanged = false;
    bool lightChanged = false;
    bool resetModelState = false;
    bool shadowMapResChanged = false;

    // Render basic GUI.
    ImVec2 vpSize = ImGui::GetMainViewport()->Size;

    // GPU profiling.
    ImVec2 wPos = ImVec2(0.01 * vpSize[0], 0.01 * vpSize[1]);
    ImVec2 wSize = ImVec2(0.15 * vpSize[0], 0.25 * vpSize[1]);
    ImGui::SetNextWindowPos(wPos, ImGuiCond_Appearing, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(wSize, ImGuiCond_Appearing);
    ImGui::Begin("Performance");
    ImGui::Text("Scene Information:");
    ImGui::Text("Light Camera : %.2f ms", m_msLightViewPass);
    ImGui::Text("Geometry Pass: %.2f ms", m_msGeometryPass);
    ImGui::Text("Light Volumes: %.2f ms", m_msLightVolumePass);
    ImGui::Text("SSAO         : %.2f ms", m_msSSAO);
    ImGui::Text("Lighting Pass: %.2f ms", m_msCombinationPass);
    ImGui::Text("Forward Pass : %.2f ms", m_msForwardPass);
    ImGui::Text("_______________________");
    ImGui::Text("Frame Time   : %.2f ms", m_msFrameTime);
    ImGui::End();

    //Settings Menu
    ImVec2 wPos1 = ImVec2(0.01 * vpSize[0], 0.28 * vpSize[1]);
    ImVec2 wSize1 = ImVec2(0.25 * vpSize[0], 0.7 * vpSize[1]);
    ImGui::SetNextWindowPos(wPos1, ImGuiCond_Appearing, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(wSize1, ImGuiCond_Appearing);
    ImGui::Begin("Settings");
    // Combo example from https://github.com/ocornut/imgui/issues/1658
    {
        std::string currentItem = DrawModeStrings[m_drawMode];
        if (ImGui::BeginCombo("DrawMode", currentItem.c_str())) {
            for (int n = 0; n < DrawModeStrings.size(); n++) {
                bool isSelected = (currentItem == DrawModeStrings[n]);
                if (ImGui::Selectable(DrawModeStrings[n].c_str(), isSelected)) {
                    currentItem = DrawModeStrings[n];
                    m_drawMode = n;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    // Texture visualization Quad.
    ImGui::Checkbox("Texture Visualization", &m_showTexVis);
    if (m_showTexVis) {
        std::string currentTexVis = TexVisStrings[m_texVisTextureIdx];
        if (ImGui::BeginCombo("Texture", currentTexVis.c_str())) {
            for (int n = 0; n < TexVisStrings.size(); n++) {
                bool isSelected = (currentTexVis == TexVisStrings[n]);
                if (ImGui::Selectable(TexVisStrings[n].c_str(), isSelected)) {
                    currentTexVis = TexVisStrings[n];
                    m_texVisTextureIdx = n;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    // Show state information of the sponza model.
    ModelState* sponzaState = m_sponzaModel->GetState();
    modelStateChanged |= ImGui::SliderFloat("Scaling", &sponzaState->scale, 0.01f, 8.0f);
    modelStateChanged |= ImGui::SliderFloat("Yaw", &m_modelYaw, 0.0f, 360.0f);
    modelStateChanged |= ImGui::SliderFloat("Pitch", &m_modelPitch, -90.0f, 90.0f);
    modelStateChanged |= ImGui::SliderFloat("Roll", &m_modelRoll, -90.0f, 90.0f);
    resetModelState |= ImGui::Button("Reset object");

    // Camera information.
    ImGui::Text("Camera:");
    ImGui::SliderFloat("X-axis", &m_viewPos.x, -10, 10);
    ImGui::SliderFloat("Y-axis", &m_viewPos.y, -10, 10);
    ImGui::SliderFloat("Z-axis", &m_viewPos.z, -10, 10);
    ImGui::SliderFloat("yaw", &m_cameraYaw, -10, 10);
    ImGui::SliderFloat("pitch", &m_cameraPitch, -1.6, 1.6);
    cameraChanged |= ImGui::Button("Reset camera");

    // Light information.
    ImGui::Text("Point Lights:");
    ImGui::Checkbox("Activate", &usePointLights);
    lightChanged |= ImGui::SliderFloat("lightAmbient", &m_lightingScales.x,
        0.0f, 1.0f);
    lightChanged |= ImGui::SliderFloat("lightDiffuse", &m_lightingScales.y,
        0.0f, 1.0f);
    lightChanged |= ImGui::SliderFloat("lightSpecular", &m_lightingScales.z,
        0.0f, 1.0f);
    ImGui::SliderFloat("shininessExp", &m_shininessExp, 1.0, 20.0f);

    // Directional light.
    ImGui::Text("Directional Light:");
    ImGui::Checkbox("Activate Shadows", &m_useShadows);
    if (m_useShadows) {
        // Resolution of shadow map.
        unsigned int currentShadowMapSizeIdx = m_shadowMapSizes[m_shadowMapSizeIdx];
        if (ImGui::BeginCombo("Resolution", std::to_string(currentShadowMapSizeIdx).c_str())) {
            for (int n = 0; n < m_shadowMapSizes.size(); n++) {
                bool isSelected = (currentShadowMapSizeIdx == m_shadowMapSizes[n]);
                if (ImGui::Selectable(std::to_string(m_shadowMapSizes[n]).c_str(), isSelected)) {
                    currentShadowMapSizeIdx = m_shadowMapSizes[n];
                    m_shadowMapSizeIdx = n;
                    shadowMapResChanged = true;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Type of shadow.
        std::string currentShadowType = m_shadowTypes[m_shadowTypeIdx];
        if (ImGui::BeginCombo("Texture", currentShadowType.c_str())) {
            for (int n = 0; n < m_shadowTypes.size(); n++) {
                bool isSelected = (currentShadowType == m_shadowTypes[n]);
                if (ImGui::Selectable(m_shadowTypes[n].c_str(), isSelected)) {
                    currentShadowType = m_shadowTypes[n];
                    m_shadowTypeIdx = n;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::SliderFloat("Light X-axis", &m_directionalLightPos.x, 0.1, 1000.0);
    ImGui::SliderFloat("Light Y-axis", &m_directionalLightPos.y, 0.1, 1000.0);
    ImGui::SliderFloat("Light Z-axis", &m_directionalLightPos.z, 0.1, 1000.0);

    // SSAO.
    ImGui::Text("SSAO Settings:");
    ImGui::Checkbox("Activate SSAO", &m_useSSAO);
    if (m_useSSAO) {
        ImGui::Checkbox("Blur Occlusion Map", &m_ssaoUseBlur);
        ImGui::SliderFloat("Bias", &m_ssaoBias, 0.0, 0.1);
        ImGui::SliderFloat("Radius", &m_ssaoRadius, 0.0, 20.0);
        ImGui::SliderInt("Kernel Size", &m_ssaoKernelSize, 1, 64);
    }

    // Other settings.
    ImGui::Text("Other Settings:");
    ImGui::Checkbox("Show origin visualization", &m_showOriginVis);


    // End of ImGui element definitions.
    ImGui::End();

    // Check if model state changed.
    if (modelStateChanged) {
        // Construct orientation from yaw, pitch and roll.
        sm::Quaternion newOrientation =
            sm::Quaternion::CreateFromYawPitchRoll(
                dx::XMConvertToRadians(m_modelYaw),
                dx::XMConvertToRadians(m_modelPitch),
                dx::XMConvertToRadians(m_modelRoll));
        newOrientation.Normalize();

        // Update model.
        ModelState* bagState = m_sponzaModel->GetState();
        bagState->orientation = newOrientation;
    }

    if (resetModelState) {
        // Reset the state of the model.
        ModelState* bagState = m_sponzaModel->GetState();
        bagState->position = { 0.0,0.0,0.0 };
        bagState->orientation = sm::Quaternion::Identity;
        bagState->scale = 1.0;
        resetModelState = false;
    }

    if (cameraChanged) {
        // Reset camera.
        m_cameraYaw = 0;
        m_cameraPitch = 0;
        m_viewPos = sm::Vector3(START_POSITION);
    }

    if (rasterizerChanged) {
        // Update rasterizer settings.
        m_rasterDesc.FillMode =
            m_showWireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
        HRESULT hr = m_d3dDevice->CreateRasterizerState(
            &m_rasterDesc, m_rasterizerState.GetAddressOf());
    }

    if (shadowMapResChanged) {
        //Resize textures.
        initShadowTextures();
    }
}


/*
 * SponzaScene::update
 */
void SponzaScene::update() {
    //Update camera parameters.
    updateCamera();

    // Updates states of all models in the scene.
    updateModels();

    // Update resources related to shadow mapping.
    updateShadows();

    // Update matrices for texture visualization quad.
    updateTexVisQuad();

    // Update resources related to the window-filling quad of the lighting pass.
    updateLightingPass();

    // #############################################################################
    // Update buffers on GPU. Assumes that all data has been updated.
    updateBuffers();
}


/*
 * SponzaScene::updateCamera
 */
void SponzaScene::updateCamera() {
    // View matrix calculation is taken from
    // https://github.com/microsoft/DirectXTK/wiki/Mouse-and-keyboard-input
    auto kb = m_controls->m_keyboard->GetState();

    // Update view direction.
    sm::Vector3 delta = sm::Vector3(float(kb.Right) - float(kb.Left), float(kb.Down)
        - float(kb.Up), 0.f) * ROTATION_GAIN;
    m_cameraPitch -= delta.y;
    m_cameraYaw -= delta.x;

    // Update camera position. TODO: Make translation relative to viewDir.
    sm::Vector3 move = sm::Vector3::Zero;
    if (kb.W) {
        move.y += 1.f;
    }

    if (kb.S) {
        move.y -= 1.f;
    }

    if (kb.A) {
        move.x += 1.f;
    }

    if (kb.D) {
        move.x -= 1.f;
    }

    if (kb.PageUp || kb.V) {
        move.z += 1.f;
    }

    if (kb.PageDown || kb.C) {
        move.z -= 1.f;
    }

    // Update camera position.
    sm::Quaternion q = sm::Quaternion::CreateFromYawPitchRoll(m_cameraYaw,
        m_cameraPitch, 0.f);
    move = sm::Vector3::Transform(move, q);
    move *= MOVEMENT_GAIN;
    m_viewPos += move;

    // Limit pitch to straight up or straight down.
    constexpr float limit = dx::XM_PIDIV2 - 0.01f;
    m_cameraPitch = std::max(-limit, m_cameraPitch);
    m_cameraPitch = std::min(+limit, m_cameraPitch);

    // Keep longitude in sane range by wrapping.
    if (m_cameraYaw > dx::XM_PI) {
        m_cameraYaw -= dx::XM_2PI;

    } else if (m_cameraYaw < -dx::XM_PI) {
        m_cameraYaw += dx::XM_2PI;
    }

    // Compute lookAt position.
    float y = sinf(m_cameraPitch);
    float r = cosf(m_cameraPitch);
    float z = r * cosf(m_cameraYaw);
    float x = r * sinf(m_cameraYaw);
    m_cameraLookAt = m_viewPos + sm::Vector3(x, y, z);
    m_viewMat = XMMatrixLookAtRH(m_viewPos, m_cameraLookAt, sm::Vector3::Up);
}


/*
 * SponzaScene::updateBuffers
 */
void SponzaScene::updateBuffers() {
    // Update matrices on GPU. TODO: Create struct on host and perform a single
    // memcpy instead.
    {
        // Update VS_CONSTANT_BUFFER.
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        // D3D11_MAP_WRITE_DISCARD requires to update ALL values.
        m_d3dContext->Map(m_sceneBufferVS.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
        VS_CONSTANT_BUFFER* dataPtr = (VS_CONSTANT_BUFFER*)mappedResource.pData;
        dataPtr->viewMat = m_viewMat.Transpose();
        dataPtr->projMat = m_projMat.Transpose();
        dataPtr->viewPos = m_viewPos;

        // For transforming frag positions into light space --> shadow mapping.
        dataPtr->lightViewMat = m_directionalLightViewMat.Transpose();
        dataPtr->lightProjMat = m_directionalLightProjectionMat.Transpose();
        m_d3dContext->Unmap(m_sceneBufferVS.Get(), 0);
    }

    {
        // Update PS_CONSTANT_BUFFER.
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        m_d3dContext->Map(m_sceneBufferPS.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
        PS_CONSTANT_BUFFER* dataPtr = (PS_CONSTANT_BUFFER*)mappedResource.pData;
 
        // Light information.
        dataPtr->lightingScales = m_lightingScales;
        dataPtr->shininessExp = m_shininessExp;

        // Visualization information.
        dataPtr->viewMat = m_viewMat.Transpose();
        dataPtr->projMat = m_projMat.Transpose();

        // Light volumes.
        dataPtr->invViewProjMat = (m_viewMat * m_projMat).Invert().Transpose();

        // Other information.
        dataPtr->drawMode = static_cast<int>(m_drawMode);
        dataPtr->viewPos = m_viewPos;
        dataPtr->pixelSize = m_pixelSize;
        m_d3dContext->Unmap(m_sceneBufferPS.Get(), 0);
    }

    {
        // Update VS_SHADOW_CONSTANT_BUFFER.
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        m_d3dContext->Map(m_shadowConstBufferVS.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
        VS_SHADOW_CONSTANT_BUFFER* dataPtr = (VS_SHADOW_CONSTANT_BUFFER*)mappedResource.pData;

        // Directional light view and projection matrix.
        dataPtr->lightViewMat = m_directionalLightViewMat.Transpose();
        dataPtr->lightProjMat = m_directionalLightProjectionMat.Transpose();
        m_d3dContext->Unmap(m_shadowConstBufferVS.Get(), 0);
    }

    {
        // Update PS_SHADOW_CONSTANT_BUFFER.
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        m_d3dContext->Map(m_shadowConstBufferPS.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
        PS_SHADOW_CONSTANT_BUFFER* dataPtr = (PS_SHADOW_CONSTANT_BUFFER*)mappedResource.pData;

        dataPtr->lightDir = -m_directionalLightPos;
        dataPtr->shadowMapSize = m_shadowMapSizes[m_shadowMapSizeIdx];
        dataPtr->lightViewMat = m_directionalLightViewMat.Transpose();
        dataPtr->lightProjMat = m_directionalLightProjectionMat.Transpose();
        dataPtr->shadowType = m_shadowTypeIdx;
        dataPtr->useShadows = m_useShadows;
        m_d3dContext->Unmap(m_shadowConstBufferPS.Get(), 0);
    }

    {
        // Update VS_MP_BUFFER.
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        m_d3dContext->Map(m_texVisConstBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
        VS_MP_BUFFER* dataPtr = (VS_MP_BUFFER*)mappedResource.pData;

        // Model and projection matrix for quad.
        dataPtr->modelMat = m_texVisModelMat.Transpose();
        dataPtr->projectionMat = m_texVisProjMat.Transpose();
        m_d3dContext->Unmap(m_texVisConstBuffer.Get(), 0);
    }

    {
        // Update PS_TexVis_BUFFER.
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        m_d3dContext->Map(m_texVisPSBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
        PS_TexVis_BUFFER* dataPtr = (PS_TexVis_BUFFER*)mappedResource.pData;

        // Extra information for visualization quad in pixel shader stage.
        dataPtr->textureIdx = m_texVisTextureIdx;
        m_d3dContext->Unmap(m_texVisPSBuffer.Get(), 0);
    }

    {
        // Update VS_MP_BUFFER for lighting pass quad.
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        m_d3dContext->Map(m_lightingPassQuadVSBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
        VS_MP_BUFFER* dataPtr = (VS_MP_BUFFER*)mappedResource.pData;

        // Model and projection matrix for quad.
        dataPtr->modelMat = m_lightingPassQuadModelMat.Transpose();
        dataPtr->projectionMat = m_lightingPassQuadProjMat.Transpose();

        // Unmap.
        m_d3dContext->Unmap(m_lightingPassQuadVSBuffer.Get(), 0);
    }

    {
        // Update VS_MP_BUFFER for SSAO.
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        m_d3dContext->Map(m_ssaoMPBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
        VS_MP_BUFFER* dataPtr = (VS_MP_BUFFER*)mappedResource.pData;

        // Model and projection matrix for quad.
        dataPtr->modelMat = m_windowQuadModelMat.Transpose();
        dataPtr->projectionMat = m_windowQuadProjMat.Transpose();
        m_d3dContext->Unmap(m_ssaoMPBuffer.Get(), 0);
    }

    {
        // Update SSAO_PARAMETER_BUFFER.
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        m_d3dContext->Map(m_ssaoParameterBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
        SSAO_PARAMETER_BUFFER* dataPtr = (SSAO_PARAMETER_BUFFER*)mappedResource.pData;

        // Extra information for visualization quad in pixel shader stage.
        dataPtr->bias=m_ssaoBias;
        dataPtr->kernelSize = m_ssaoKernelSize;
        dataPtr->radius = m_ssaoRadius;
        dataPtr->useSSAO = m_useSSAO;

        m_d3dContext->Unmap(m_ssaoParameterBuffer.Get(), 0);
    }
}


/*
 * SponzaScene::updateModels
 */
void SponzaScene::updateModels() {
    // Update skybox cube position to match camera position. Keeps skybox
    // always infinetly far away.
    ModelState* skyBoxState = m_skyBoxCube->GetState();
    skyBoxState->position = m_viewPos;
}


/*
 * SponzaScene::updateTexVisQuad
 */
void SponzaScene::updateTexVisQuad() {
    // Make sure that the width and height of the quad are always the same.
    float adjustedHeight = m_aspectRatio * (m_wHeight / 2.0);
    float offsetHeight = adjustedHeight - (m_wHeight / 2.0);
    sm::Matrix shiftUpMat = sm::Matrix::CreateTranslation(
        sm::Vector3(0.0, offsetHeight, 0.0));

    // Model matrix for shader.
    m_texVisModelMat = sm::Matrix::CreateScale(
        sm::Vector3(m_wWidth / 2.0,
        m_aspectRatio * (m_wHeight / 2.0),
        1.0)) * shiftUpMat;

    // Perform orthographic projection for texture visualization quad onto the screen.
    m_texVisProjMat = sm::Matrix::CreateOrthographic(m_wWidth,
        m_wHeight, 0.0, 1.0f);
}


/*
 * SponzaScene::updateShadows
 */
void SponzaScene::updateShadows() {
    // Frustum corners of camera in normalized device coordinates (NDC). In D3D11
    // the depth range is 0-1!
    const std::array<sm::Vector4,8> ndcCorners = {
        sm::Vector4(-1, -1, 0, 1), // Near bottom-left
        sm::Vector4(1, -1, 0, 1), // Near bottom-right
        sm::Vector4(-1,  1, 0, 1), // Near top-left
        sm::Vector4(1,  1, 0, 1), // Near top-right
        sm::Vector4(-1, -1, 1, 1), // Far bottom-left
        sm::Vector4(1, -1, 1, 1), // Far bottom-right
        sm::Vector4(-1,  1, 1, 1), // Far top-left
        sm::Vector4(1,  1, 1, 1)  // Far top-right
    };

    // Compute the combined view-projection matrix.
    sm::Matrix viewProjectionMatrix = m_viewMat * m_projMat;

    // Invert the combined matrix
    sm::Matrix invertedViewProjectionMatrix = viewProjectionMatrix.Invert();

    // Transform NDC frustum corners to world space.
    std::array<sm::Vector4, 8> worldSpaceCorners;
    for (int i = 0; i < 8; i++) {
        sm::Vector4 ndcCorner = ndcCorners[i];
        sm::Vector4 worldSpaceCorner = sm::Vector4::Transform(ndcCorner,
            invertedViewProjectionMatrix);
        worldSpaceCorners[i] = worldSpaceCorner / worldSpaceCorner.w;
    }

    // Compute centroid of the frustum (world space).
    sm::Vector3 centroid;
    for (sm::Vector4& corner : worldSpaceCorners) {
        centroid += sm::Vector3(corner.x, corner.y, corner.z);
    }
    centroid = centroid / 8.0;

    // Compute temporary working position for the light.
    sm::Vector3 lightDir = dx::XMVector3Normalize(-m_directionalLightPos);  // TODO: Separate.
    sm::Vector3 tempPosition = centroid - m_cameraFarClip * lightDir; // Go towards light.

    // lookAt view matrix that looks towards centroid.
    sm::Matrix lightViewMat = sm::Matrix::CreateLookAt(
        tempPosition, centroid,
        sm::Vector3(0.0, 1.0, 0.0));

    // Transform world space frustum corners into view space of light.
    // At the same time find Min and Max X, Y and Z.
    std::array<sm::Vector4, 8> lightViewSpaceCorners;
    float minX = 0.0;
    float maxX = 0.0;
    float minY = 0.0;
    float maxY = 0.0;
    float minZ = 0.0;
    float maxZ = 0.0;
    for (int i = 0; i < 8; i++) {
        // Transform frustum corners from world space to light view space.
        sm::Vector4 worldCorner = worldSpaceCorners[i];
        lightViewSpaceCorners[i] = sm::Vector4::Transform(worldCorner, lightViewMat);

        // Compute Min and Max X,Y,Z.
        minX = std::min(minX, lightViewSpaceCorners[i].x);
        maxX = std::max(maxX, lightViewSpaceCorners[i].x);
        
        minY = std::min(minY, lightViewSpaceCorners[i].y);
        maxY = std::max(maxY, lightViewSpaceCorners[i].y);
        
        minZ = std::min(minZ, lightViewSpaceCorners[i].z);
        maxZ = std::max(maxZ, lightViewSpaceCorners[i].z);
    }

    // Compute off center orthographic projection matrix.
    sm::Matrix offCenterProj = sm::Matrix::CreateOrthographicOffCenter(minX, maxX,
        minY, maxY, maxZ, -minZ);

    // Set computed matrices.
    m_directionalLightViewMat = lightViewMat;
    m_directionalLightProjectionMat = offCenterProj;
}


/*
 * SponzaScene::updateLightingPass
 */
void SponzaScene::updateLightingPass() {
    // Model matrix for shader.
    m_lightingPassQuadModelMat = sm::Matrix::CreateScale(
        sm::Vector3(m_wWidth / 2.0, (m_wHeight / 2.0),1.0));

    // Perform orthographic projection for texture visualization quad onto the screen.
    m_lightingPassQuadProjMat = sm::Matrix::CreateOrthographic(m_wWidth,
        m_wHeight, 0.0, 1.0f);
}


/*
 * SponzaScene::updateSSAO
 */
void SponzaScene::updateSSAO() {
    // Model matrix for shader.
    m_windowQuadModelMat = sm::Matrix::CreateScale(
        sm::Vector3(m_wWidth / 2.0, (m_wHeight / 2.0), 1.0));

    // Perform orthographic projection onto the screen.
    m_windowQuadProjMat = sm::Matrix::CreateOrthographic(m_wWidth,
        m_wHeight, 0.0, 1.0f);
}


/*
 * SponzaScene::initGBuffer
 */
void SponzaScene::initGBuffer() {
    // Normals. 2 channels for Octahedron-normal vectors.
    {
        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = m_wWidth;
        textureDesc.Height = m_wHeight;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = 0;
        HRESULT hr = m_d3dDevice->CreateTexture2D(&textureDesc, nullptr,
            m_gBufferTextures[0].GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a RENDERTARGET view on the texture (used in geometry pass).
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = textureDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        hr = m_d3dDevice->CreateRenderTargetView(m_gBufferTextures[0].Get(),
            &rtvDesc, m_gBufferRTVs[0].GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a SHADER RESOURCE view on the texture (used in lighting pass).
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {}; // Fill in SRV description
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        hr = m_d3dDevice->CreateShaderResourceView(m_gBufferTextures[0].Get(),
            &srvDesc, m_gBufferSRVs[0].GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    // Diffuse Albedo. 8 bit per channel should be enough.
    {
        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = m_wWidth;
        textureDesc.Height = m_wHeight;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 4-channel texture
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = 0;
        HRESULT hr = m_d3dDevice->CreateTexture2D(&textureDesc, nullptr,
            m_gBufferTextures[1].GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a RENDERTARGET view on the texture (used in geometry pass).
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = textureDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        hr = m_d3dDevice->CreateRenderTargetView(m_gBufferTextures[1].Get(),
            &rtvDesc, m_gBufferRTVs[1].GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a SHADER RESOURCE view on the texture (used in lighting pass).
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {}; // Fill in SRV description
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        hr = m_d3dDevice->CreateShaderResourceView(m_gBufferTextures[1].Get(),
            &srvDesc, m_gBufferSRVs[1].GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    {
        //Create G-buffer depth texture. Will be 32 bit per pixel no matter what.
        D3D11_TEXTURE2D_DESC shadowMapDesc;
        ZeroMemory(&shadowMapDesc, sizeof(D3D11_TEXTURE2D_DESC));
        shadowMapDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        shadowMapDesc.MipLevels = 1;
        shadowMapDesc.ArraySize = 1;
        shadowMapDesc.SampleDesc.Count = 1;
        shadowMapDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
        shadowMapDesc.Height = m_wHeight;
        shadowMapDesc.Width = m_wWidth;
        HRESULT hr = m_d3dDevice->CreateTexture2D(
            &shadowMapDesc,
            nullptr,
            m_gBufferDepthTexture.GetAddressOf()
        );
        assert(SUCCEEDED(hr));

        // Create depth stencil view.
        D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
        ZeroMemory(&depthStencilViewDesc, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
        depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depthStencilViewDesc.Texture2D.MipSlice = 0;

        hr = m_d3dDevice->CreateDepthStencilView(
            m_gBufferDepthTexture.Get(),
            &depthStencilViewDesc,
            m_gBufferDepthStencilView.GetAddressOf()
        );
        assert(SUCCEEDED(hr));

        // Create SRV for the depth texture.
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        ZeroMemory(&shaderResourceViewDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;
        hr = m_d3dDevice->CreateShaderResourceView(
            m_gBufferDepthTexture.Get(),
            &shaderResourceViewDesc,
            m_gBufferDepthSRV.GetAddressOf()
        );
        assert(SUCCEEDED(hr));
    }

    // Create two textures for the lighting calculations. Will be used when
    // light volumes (spheres, ...).
    for (int i = 0; i < 2; ++i) {
        // Create a texture.
        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = m_wWidth;
        textureDesc.Height = m_wHeight;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 4-channel texture
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = 0;
        HRESULT hr = m_d3dDevice->CreateTexture2D(&textureDesc, nullptr,
            m_lightingTextures[i].GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a RENDERTARGET view on the texture (used in geometry pass).
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = textureDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        hr = m_d3dDevice->CreateRenderTargetView(m_lightingTextures[i].Get(),
            &rtvDesc, m_lightingTextureRTVs[i].GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a SHADER RESOURCE view on the texture (used in lighting pass).
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {}; // Fill in SRV description
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        hr = m_d3dDevice->CreateShaderResourceView(m_lightingTextures[i].Get(),
            &srvDesc, m_lightingTextureSRVs[i].GetAddressOf());
        assert(SUCCEEDED(hr));
    }
}


/*
 * SponzaScene::initShadows
 */
void SponzaScene::initShadows() {
    // Init textures, views and viewport for requested texture size.
    initShadowTextures();

    // Create comparison state. When the texture gets sampled outside the borders,
    // return 1, which will indicate that there is no shadow.
    D3D11_SAMPLER_DESC comparisonSamplerDesc;
    ZeroMemory(&comparisonSamplerDesc, sizeof(D3D11_SAMPLER_DESC));
    comparisonSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    comparisonSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    comparisonSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    comparisonSamplerDesc.BorderColor[0] = 1.0f;
    comparisonSamplerDesc.BorderColor[1] = 1.0f;
    comparisonSamplerDesc.BorderColor[2] = 1.0f;
    comparisonSamplerDesc.BorderColor[3] = 1.0f;
    comparisonSamplerDesc.MinLOD = 0.f;
    comparisonSamplerDesc.MaxLOD = 0;   // We have only 1 mip level anyway.
    comparisonSamplerDesc.MipLODBias = 0.f;
    comparisonSamplerDesc.MaxAnisotropy = 0;
    comparisonSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    comparisonSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    HRESULT hr = m_d3dDevice->CreateSamplerState(
        &comparisonSamplerDesc,
        &m_comparisonSampler_point);
    assert(SUCCEEDED(hr));

    // Create render states. Feature level 9_1 requires DepthClipEnable == true.
    D3D11_RASTERIZER_DESC drawingRenderStateDesc;
    ZeroMemory(&drawingRenderStateDesc, sizeof(D3D11_RASTERIZER_DESC));
    drawingRenderStateDesc.CullMode = D3D11_CULL_BACK;
    drawingRenderStateDesc.FillMode = D3D11_FILL_SOLID;
    drawingRenderStateDesc.FrontCounterClockwise = true;
    drawingRenderStateDesc.DepthClipEnable = true;
    hr = m_d3dDevice->CreateRasterizerState(
        &drawingRenderStateDesc,
        m_rasterizerStateShadows.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create view and projection matrix for directional light.
    m_directionalLightProjectionMat = sm::Matrix::Identity;
    m_directionalLightViewMat = sm::Matrix::Identity;

    {
        // Create shadow constant buffer for vertex shader.
        VS_SHADOW_CONSTANT_BUFFER constBufferData;
        constBufferData.lightViewMat = m_directionalLightViewMat.Transpose();
        constBufferData.lightProjMat = m_directionalLightProjectionMat.Transpose();

        // Fill in a buffer description.
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = sizeof(VS_SHADOW_CONSTANT_BUFFER);
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
        hr = m_d3dDevice->CreateBuffer(&cbDesc, &constInitData,
            m_shadowConstBufferVS.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    {
        // Create shadow constant buffer for pixel shader.
        PS_SHADOW_CONSTANT_BUFFER constBufferData;
        constBufferData.lightDir = -m_directionalLightPos;  // TODO: Will change in the future.
        constBufferData.shadowMapSize = m_shadowMapSizes[m_shadowMapSizeIdx];
        constBufferData.lightViewMat = m_directionalLightViewMat.Transpose();
        constBufferData.lightProjMat = m_directionalLightProjectionMat.Transpose();
        constBufferData.shadowType = m_shadowTypeIdx;
        constBufferData.useShadows = m_useShadows;

        // Fill in a buffer description.
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = sizeof(PS_SHADOW_CONSTANT_BUFFER);
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
        hr = m_d3dDevice->CreateBuffer(&cbDesc, &constInitData,
            m_shadowConstBufferPS.GetAddressOf());
        assert(SUCCEEDED(hr));
    }
        
    // Create depth stencil state.
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; // Allow depth writes
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS; // Always pass depth test
    depthStencilDesc.StencilEnable = false; // No stencil operations

    hr = m_d3dDevice->CreateDepthStencilState(&depthStencilDesc,
        m_shadowDepthStencilState.GetAddressOf());
    assert(SUCCEEDED(hr));
}


/*
 * SponzaScene::initShadowTextures
 */
void SponzaScene::initShadowTextures() {
    // Reset existing textures.
    m_shadowMap.Reset();
    m_shadowDepthView.Reset();
    m_shadowResourceView.Reset();

    // Init viewport for shadow rendering. TODO: Use in the future.
    ZeroMemory(&m_shadowViewport, sizeof(D3D11_VIEWPORT));
    m_shadowViewport.Height = m_shadowMapSizes[m_shadowMapSizeIdx];
    m_shadowViewport.Width = m_shadowMapSizes[m_shadowMapSizeIdx];
    m_shadowViewport.MinDepth = 0.f;
    m_shadowViewport.MaxDepth = 1.f;

    //Create depth buffer.
    D3D11_TEXTURE2D_DESC shadowMapDesc;
    ZeroMemory(&shadowMapDesc, sizeof(D3D11_TEXTURE2D_DESC));
    shadowMapDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    shadowMapDesc.MipLevels = 1;
    shadowMapDesc.ArraySize = 1;
    shadowMapDesc.SampleDesc.Count = 1;
    shadowMapDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    shadowMapDesc.Height = static_cast<UINT>(m_shadowMapSizes[m_shadowMapSizeIdx]);
    shadowMapDesc.Width = static_cast<UINT>(m_shadowMapSizes[m_shadowMapSizeIdx]);
    HRESULT hr = m_d3dDevice->CreateTexture2D(
        &shadowMapDesc,
        nullptr,
        m_shadowMap.GetAddressOf()
    );
    assert(SUCCEEDED(hr));

    // Create depth stencil view.
    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
    ZeroMemory(&depthStencilViewDesc, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
    depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Texture2D.MipSlice = 0;

    hr = m_d3dDevice->CreateDepthStencilView(
        m_shadowMap.Get(),
        &depthStencilViewDesc,
        m_shadowDepthView.GetAddressOf()
    );
    assert(SUCCEEDED(hr));

    // Create SRV for the depth texture.
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    ZeroMemory(&shaderResourceViewDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    shaderResourceViewDesc.Texture2D.MipLevels = 1;
    hr = m_d3dDevice->CreateShaderResourceView(
        m_shadowMap.Get(),
        &shaderResourceViewDesc,
        m_shadowResourceView.GetAddressOf()
    );
    assert(SUCCEEDED(hr));

}


/*
 * SponzaScene::initLights
 */
void SponzaScene::initLights() {
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::seed_seq seedSequence = { m_seedValue };
    std::default_random_engine generator(seedSequence);

    for (unsigned int i = 0; i < m_NR_LIGHTS; i++) {
        // Generate position.
        sm::Vector3 posSample = sm::Vector3(
            randomFloats(generator),
            randomFloats(generator),
            randomFloats(generator)
        );

        // Sponza relevant area for lights:
        // X=[139,-111], Y=[-6,23],Z=[54,-49]
        float posX = -111.0 + posSample.x * (139.0 - (-111.0));
        float posY = -6 + posSample.y * (23.0 - (-6.0));
        float posZ = -49 + posSample.z * (54.0 - (-49));

        // Generate color.
        sm::Vector3 colorSample = sm::Vector3(
            randomFloats(generator),
            randomFloats(generator),
            randomFloats(generator)
        );
        //colorSample.Normalize();    // To ensure bright colors.

        // Store the light.
        Light newLight;
        newLight.Position = sm::Vector4(posX, posY, posZ, 1.0);
        newLight.Color = sm::Vector4(colorSample.x, colorSample.y, colorSample.z, 1.0);
        newLight.Scale = sm::Vector4(32.0, 32.0, 32.0, 1.0);
        m_lights.push_back(newLight);
    }

    D3D11_BUFFER_DESC bufferDesc;
    ZeroMemory(&bufferDesc, sizeof(bufferDesc));
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(Light) * m_NR_LIGHTS;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;

    // Initialize the buffer with the light data.
    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(initData));
    initData.pSysMem = m_lights.data();

    // Create the constant buffer.
    HRESULT hr = m_d3dDevice->CreateBuffer(&bufferDesc, &initData,
        m_lightPosColorBuffer.GetAddressOf());

}


/*
 * SponzaScene::initTextureVisualization
 */
void SponzaScene::initTextureVisualization() {
    // Scale the texture visualization quad, so its symmetrical.
    m_texVisModelMat = sm::Matrix::Identity;

    // Perform orthographic projection.
    m_texVisProjMat = sm::Matrix::Identity;

    // Create MP constant buffer.
    VS_MP_BUFFER mpBufferData;
    mpBufferData.modelMat = m_texVisModelMat.Transpose();
    mpBufferData.projectionMat = m_texVisProjMat.Transpose();

    // Fill in a buffer description.
    D3D11_BUFFER_DESC mpBufferDesc;
    mpBufferDesc.ByteWidth = sizeof(VS_MP_BUFFER);
    mpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    mpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    mpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    mpBufferDesc.MiscFlags = 0;
    mpBufferDesc.StructureByteStride = 0;

    // Fill in the subresource data.
    D3D11_SUBRESOURCE_DATA mpInitData;
    mpInitData.pSysMem = &mpBufferData;
    mpInitData.SysMemPitch = 0;
    mpInitData.SysMemSlicePitch = 0;

    // Create the buffer.
    HRESULT hr = m_d3dDevice->CreateBuffer(&mpBufferDesc, &mpInitData,
        m_texVisConstBuffer.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create the PS constant buffer.
    PS_TexVis_BUFFER psTexVisBufferData;
    psTexVisBufferData.textureIdx = 0;

    // Fill in a buffer description.
    D3D11_BUFFER_DESC psTexVisBufferDesc;
    psTexVisBufferDesc.ByteWidth = sizeof(PS_TexVis_BUFFER);
    psTexVisBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    psTexVisBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    psTexVisBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    psTexVisBufferDesc.MiscFlags = 0;
    psTexVisBufferDesc.StructureByteStride = 0;

    // Fill in the subresource data.
    D3D11_SUBRESOURCE_DATA psTexVisBufferInitData;
    psTexVisBufferInitData.pSysMem = &psTexVisBufferData;
    psTexVisBufferInitData.SysMemPitch = 0;
    psTexVisBufferInitData.SysMemSlicePitch = 0;

    // Create the PS buffer.
    hr = m_d3dDevice->CreateBuffer(&psTexVisBufferDesc,
        &psTexVisBufferInitData, m_texVisPSBuffer.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create texture visualization quad.In Direct3D, the origin (0, 0) for textures
    //  is typically at the top-left corner.
    std::vector<Vertex> texVisVertices = {
        // Bottom left.
        Vertex(sm::Vector3(0.3f, -1.0f, 0.0f), sm::Vector2(0.0f, 1.0f)),

        // Bottom right.
        Vertex(sm::Vector3(1.0f, -1.0f, 0.0f), sm::Vector2(1.0f, 1.0f)),

        // Top left.
        Vertex(sm::Vector3(0.3f, -0.3f, 0.0f), sm::Vector2(0.0f, 0.0f)),

        // Top right.
        Vertex(sm::Vector3(1.0f, -0.3f, 0.0f), sm::Vector2(1.0f, 0.0f)),
    };
    std::vector<unsigned int> texVisIndices = { 0, 1, 2, 2, 1, 3 };

    // We dont care about the material.
    Material texVisMaterial;
    texVisMaterial.matAmbientColor = sm::Vector3(1.0, 0.0, 0.0);
    texVisMaterial.matDiffuseColor = sm::Vector3(1.0, 0.0, 0.0);
    texVisMaterial.matSpecularColor = sm::Vector3(1.0, 0.0, 0.0);
    texVisMaterial.matShininess = 1.0f;
    texVisMaterial.matOpticalDensity = 1.0f;
    texVisMaterial.matDissolveFactor = 1.0f;
    texVisMaterial.matColorViaTex = false;
    texVisMaterial.matPadding0 = 0.0f;
    texVisMaterial.matPadding1 = 0.0f;
    texVisMaterial.matPadding2 = 0.0f;

    // We set textures from the outside.
    std::vector<D3D11_INPUT_ELEMENT_DESC> texVisVertexLayout = {};

    // Create ModelClass object.
    m_texVisQuad = std::make_shared<ModelClass>(
        texVisVertices,
        texVisIndices,
        texVisVertexLayout,
        texVisMaterial,
        m_d3dDevice,
        m_d3dContext, &m_viewMat, 5, sm::Vector3{ 0.0, 0.0, 0.0 },
        sm::Vector4{ 0.0, 0.0, 0.0, 1.0 }, L"\\src\\shader\\Quad_vs.hlsl",
        L"\\src\\shader\\Quad_ps.hlsl");
}


/*
 * SponzaScene::initLightingPass
 */
void SponzaScene::initLightingPass() {
    // Scale the quad according to the current viewport size.
    m_lightingPassQuadModelMat = sm::Matrix::Identity;

    // Perform orthographic projection.
    m_lightingPassQuadProjMat = sm::Matrix::Identity;

    // Create MP constant buffer.
    VS_MP_BUFFER mpBufferData;
    mpBufferData.modelMat = m_lightingPassQuadModelMat.Transpose();
    mpBufferData.projectionMat = m_lightingPassQuadProjMat.Transpose();

    // Fill in a buffer description.
    D3D11_BUFFER_DESC mpBufferDesc;
    mpBufferDesc.ByteWidth = sizeof(VS_MP_BUFFER);
    mpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    mpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    mpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    mpBufferDesc.MiscFlags = 0;
    mpBufferDesc.StructureByteStride = 0;

    // Fill in the subresource data.
    D3D11_SUBRESOURCE_DATA mpInitData;
    mpInitData.pSysMem = &mpBufferData;
    mpInitData.SysMemPitch = 0;
    mpInitData.SysMemSlicePitch = 0;

    // Create the buffer.
    HRESULT hr = m_d3dDevice->CreateBuffer(&mpBufferDesc, &mpInitData,
        m_lightingPassQuadVSBuffer.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create window-filling quad. In Direct3D, the origin (0, 0) for textures is
    // typically at the top-left corner.
    std::vector<Vertex> windowQuadVertices = {
        // Bottom left.
        Vertex(sm::Vector3(-1.0f, -1.0f, 0.0f), sm::Vector2(0.0f, 1.0f)),

        // Bottom right.
        Vertex(sm::Vector3(1.0f, -1.0f, 0.0f),  sm::Vector2(1.0f, 1.0f)),

        // Top left.
        Vertex(sm::Vector3(-1.0f, 1.0f, 0.0f),  sm::Vector2(0.0f, 0.0f)),

        // Top right.
        Vertex(sm::Vector3(1.0f, 1.0f, 0.0f),   sm::Vector2(1.0f, 0.0f)),
    };
    std::vector<unsigned int> windowQuadIndices = { 0, 1, 2, 2, 1, 3 };

    // We dont care about the material.
    Material windowQuadMaterial;
    windowQuadMaterial.matAmbientColor = sm::Vector3(1.0, 0.0, 0.0);
    windowQuadMaterial.matDiffuseColor = sm::Vector3(1.0, 0.0, 0.0);
    windowQuadMaterial.matSpecularColor = sm::Vector3(1.0, 0.0, 0.0);
    windowQuadMaterial.matShininess = 1.0f;
    windowQuadMaterial.matOpticalDensity = 1.0f;
    windowQuadMaterial.matDissolveFactor = 1.0f;
    windowQuadMaterial.matColorViaTex = false;
    windowQuadMaterial.matPadding0 = 0.0f;
    windowQuadMaterial.matPadding1 = 0.0f;
    windowQuadMaterial.matPadding2 = 0.0f;

    // We set textures from the outside.
    std::vector<D3D11_INPUT_ELEMENT_DESC> windowQuadVertexLayout = {};

    // Create ModelClass object.
    m_lightingPassQuad = std::make_shared<ModelClass>(
        windowQuadVertices,
        windowQuadIndices,
        windowQuadVertexLayout,
        windowQuadMaterial,
        m_d3dDevice,
        m_d3dContext, &m_viewMat, 5, sm::Vector3{ 0.0, 0.0, 0.0 },
        sm::Vector4{ 0.0, 0.0, 0.0, 1.0 }, L"\\src\\shader\\LightingPass_vs.hlsl",
        L"\\src\\shader\\LightingPass_ps.hlsl");

    // TODO: Improve this function.
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;   // Source blending factor
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;  // Destination blending factor
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;  // Blending operation
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; // Enable writing to all color channels
    hr = m_d3dDevice->CreateBlendState(&blendDesc, m_additiveBlendState.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create the rasterizer state for light volume rendering.
    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_BACK; // Cull front-facing triangles
    rasterizerDesc.DepthClipEnable = true;  // default is true
    m_d3dDevice->CreateRasterizerState(&rasterizerDesc,
        m_rasterizerStateLightVolumes.GetAddressOf());
}


/*
 * SponzaScene::initSceneBuffers
 */
void SponzaScene::initSceneBuffers() {
    // Create constant buffer.
    VS_CONSTANT_BUFFER constBufferData;
    constBufferData.viewMat = m_viewMat.Transpose();
    constBufferData.projMat = m_projMat.Transpose();
    constBufferData.viewPos = m_viewPos;
    constBufferData.padding0 = 0;
    constBufferData.lightViewMat = m_directionalLightViewMat.Transpose();
    constBufferData.lightProjMat = m_directionalLightProjectionMat.Transpose();

    // Fill in a buffer description.
    D3D11_BUFFER_DESC cbDesc;
    cbDesc.ByteWidth = sizeof(VS_CONSTANT_BUFFER);
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
        m_sceneBufferVS.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Create constant buffer for pixel shader.
    PS_CONSTANT_BUFFER constBufferPSData = {};
    constBufferPSData.lightingScales = m_lightingScales;
    constBufferPSData.shininessExp = m_shininessExp;
    constBufferPSData.viewMat = m_viewMat.Transpose();
    constBufferPSData.projMat = m_projMat.Transpose();
    constBufferPSData.invViewProjMat = (m_viewMat * m_projMat).Invert().Transpose();
    constBufferPSData.drawMode = static_cast<int>(m_drawMode);
    constBufferPSData.viewPos = m_viewPos;
    constBufferPSData.pixelSize = m_pixelSize;



    // Fill in a buffer description.
    D3D11_BUFFER_DESC cbPSDesc;
    cbPSDesc.ByteWidth = sizeof(PS_CONSTANT_BUFFER);
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
        m_sceneBufferPS.GetAddressOf());
    assert(SUCCEEDED(hr));
}


/*
 * SponzaScene::initSkyBox
 */
void SponzaScene::initSkyBox() {
    // Create cube model. Scale does not matter for cube box since the cube
    // will move with the camera anway.
    m_skyBoxCube = std::make_shared<ModelClass>(ModelClass::BaseType::CUBE,
        sm::Vector3(0.0, 1.0, 0.0), m_d3dDevice, m_d3dContext,
        &m_viewMat, 1, sm::Vector3{ 0.0, 0.0, 0.0 },
        sm::Vector4{ 0.0, 0.0, 0.0, 1.0 }, L"\\src\\shader\\SkyBox_vs.hlsl",
        L"\\src\\shader\\SkyBox_ps.hlsl");

    // Load cube map from disk.
    std::wstring skyBoxTexturePath = Helper::ConvertUtf8ToWide(
        Helper::GetAssetFullPathString(
            "\\assets\\textures\\skybox\\learnopengl.dds"));
    HRESULT hr = dx::CreateDDSTextureFromFile(m_d3dDevice.Get(),
        m_d3dContext.Get(), skyBoxTexturePath.c_str(), nullptr,
        m_skyBoxTexture.srv.GetAddressOf());
    assert(SUCCEEDED(hr));
}


/*
 * SponzaScene::lerp
 */
float SponzaScene::lerp(float a, float b, float f) {
    return a + f * (b - a);
}


/*
 * SponzaScene::initSSAO
 */
void SponzaScene::initSSAO() {
    // Init texture sampler settings. 
    {
        //This ensures we don't accidentally oversample position/depth values in
        // screen-space outside the texture's default coordinate region.
        D3D11_SAMPLER_DESC samplerDesc;
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;// NN.
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX; // Use all mip maps.
        HRESULT hr = m_d3dDevice->CreateSamplerState(
            &samplerDesc, m_gBufferSampler.GetAddressOf());
        assert(SUCCEEDED(hr));
    }
    {
        // Tiled textures.
        D3D11_SAMPLER_DESC samplerDesc;
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;// NN.
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;  // Repeating pattern.
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;  // Repeating pattern.
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;  // Repeating pattern.
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX; // Use all mip maps.
        HRESULT hr = m_d3dDevice->CreateSamplerState(
            &samplerDesc, m_tiledTextureSampler.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    // Create hemishphere sample kernel.
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::seed_seq seedSequence = { m_seedValue };
    std::default_random_engine generator(seedSequence);
    std::vector<sm::Vector4> ssaoKernel;
    for (unsigned int i = 0; i < 64; ++i) {
        sm::Vector3 sample(
            randomFloats(generator) * 2.0 - 1.0,    // -1 to 1
            randomFloats(generator) * 2.0 - 1.0,    // -1 to 1
            randomFloats(generator)                 // 0 to 1
        );
        sample.Normalize();

        float scale = (float)i / 64.0;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sm::Vector4(sample.x,sample.y,sample.z,1.0));
    }

    // Create hemisphere sample kernel constant buffer.
    {
        D3D11_BUFFER_DESC bufferDesc;
        ZeroMemory(&bufferDesc, sizeof(bufferDesc));
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(SamplePosition) * ssaoKernel.size();
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = 0;

        // Initialize the buffer with the light data.
        D3D11_SUBRESOURCE_DATA initData;
        ZeroMemory(&initData, sizeof(initData));
        initData.pSysMem = ssaoKernel.data();

        // Create the constant buffer.
        HRESULT hr = m_d3dDevice->CreateBuffer(&bufferDesc, &initData,
            m_hemisphereKernelBuffer.GetAddressOf());
    }

    // Create 4x4 random rotation vectors.
    std::vector<sm::Vector4> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++) {
        sm::Vector4 noise(
            randomFloats(generator) * 2.0 - 1.0,    // -1 to 1
            randomFloats(generator) * 2.0 - 1.0,    // -1 to 1
            0.0f, 0.0f);
        ssaoNoise.push_back(noise);
    }

    // Create 4x4 random rotation texture.
    {
        D3D11_TEXTURE2D_DESC randomVectorTextureDesc = {};
        randomVectorTextureDesc.Width = 4;
        randomVectorTextureDesc.Height = 4;
        randomVectorTextureDesc.MipLevels = 1;
        randomVectorTextureDesc.ArraySize = 1;
        randomVectorTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        randomVectorTextureDesc.SampleDesc.Count = 1;
        randomVectorTextureDesc.SampleDesc.Quality = 0;
        randomVectorTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        randomVectorTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // Bind as shader resource.
        randomVectorTextureDesc.CPUAccessFlags = 0;
        randomVectorTextureDesc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA initialData = {};
        initialData.pSysMem = ssaoNoise.data();
        initialData.SysMemPitch = sizeof(sm::Vector4) * 4;

        // Create the texture.
        HRESULT hr = m_d3dDevice->CreateTexture2D(&randomVectorTextureDesc, &initialData,
            m_randomVectorTexture.GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a SHADER RESOURCE view on the texture.
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {}; // Fill in SRV description
        srvDesc.Format = randomVectorTextureDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        hr = m_d3dDevice->CreateShaderResourceView(m_randomVectorTexture.Get(),
            &srvDesc, m_randomVectorTextureSRV.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    // Create texture visualization quad.In Direct3D, the origin (0, 0) for textures
    //  is typically at the top-left corner.
    {
        std::vector<Vertex> windowQuadVertices = {
            // Bottom left.
            Vertex(sm::Vector3(-1.0f, -1.0f, 0.0f), sm::Vector2(0.0f, 1.0f)),

            // Bottom right.
            Vertex(sm::Vector3(1.0f, -1.0f, 0.0f), sm::Vector2(1.0f, 1.0f)),

            // Top left.
            Vertex(sm::Vector3(-1.0f, 1.0f, 0.0f), sm::Vector2(0.0f, 0.0f)),

            // Top right.
            Vertex(sm::Vector3(1.0f, 1.0f, 0.0f), sm::Vector2(1.0f, 0.0f)),
        };
        std::vector<unsigned int> windowQuadIndices = { 0, 1, 2, 2, 1, 3 };

        // We dont care about the material.
        Material windowQuadMaterial;
        windowQuadMaterial.matAmbientColor = sm::Vector3(1.0, 0.0, 0.0);
        windowQuadMaterial.matDiffuseColor = sm::Vector3(1.0, 0.0, 0.0);
        windowQuadMaterial.matSpecularColor = sm::Vector3(1.0, 0.0, 0.0);
        windowQuadMaterial.matShininess = 1.0f;
        windowQuadMaterial.matOpticalDensity = 1.0f;
        windowQuadMaterial.matDissolveFactor = 1.0f;
        windowQuadMaterial.matColorViaTex = false;
        windowQuadMaterial.matPadding0 = 0.0f;
        windowQuadMaterial.matPadding1 = 0.0f;
        windowQuadMaterial.matPadding2 = 0.0f;

        // We set textures from the outside.
        std::vector<D3D11_INPUT_ELEMENT_DESC> windowQuadVertexLayout = {};

        // Create window filling quads.
        m_ssaoQuad = std::make_shared<ModelClass>(
            windowQuadVertices,
            windowQuadIndices,
            windowQuadVertexLayout,
            windowQuadMaterial,
            m_d3dDevice,
            m_d3dContext, &m_viewMat, 1, sm::Vector3{ 0.0, 0.0, 0.0 },
            sm::Vector4{ 0.0, 0.0, 0.0, 1.0 }, L"\\src\\shader\\SSAO_vs.hlsl",
            L"\\src\\shader\\SSAO_ps.hlsl");

        // Quad that uses blur shaders.
        m_ssaoQuadBlur = std::make_shared<ModelClass>(
            windowQuadVertices,
            windowQuadIndices,
            windowQuadVertexLayout,
            windowQuadMaterial,
            m_d3dDevice,
            m_d3dContext, &m_viewMat, 1, sm::Vector3{ 0.0, 0.0, 0.0 },
            sm::Vector4{ 0.0, 0.0, 0.0, 1.0 }, L"\\src\\shader\\Blur_vs.hlsl",
            L"\\src\\shader\\Blur_ps.hlsl");
    }
    
    // Model matrix for shader.
    m_windowQuadModelMat = sm::Matrix::CreateScale(
        sm::Vector3(m_wWidth / 2.0, (m_wHeight / 2.0), 1.0));

    // Perform orthographic projection onto the screen.
    m_windowQuadProjMat = sm::Matrix::CreateOrthographic(m_wWidth,
        m_wHeight, 0.0, 1.0f);

    {
        // Create MP constant buffer.
        VS_MP_BUFFER mpBufferData;
        mpBufferData.modelMat = m_windowQuadModelMat.Transpose();
        mpBufferData.projectionMat = m_windowQuadProjMat.Transpose();

        // Fill in a buffer description.
        D3D11_BUFFER_DESC mpBufferDesc;
        mpBufferDesc.ByteWidth = sizeof(VS_MP_BUFFER);
        mpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        mpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        mpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        mpBufferDesc.MiscFlags = 0;
        mpBufferDesc.StructureByteStride = 0;

        // Fill in the subresource data.
        D3D11_SUBRESOURCE_DATA mpInitData;
        mpInitData.pSysMem = &mpBufferData;
        mpInitData.SysMemPitch = 0;
        mpInitData.SysMemSlicePitch = 0;

        // Create the buffer.
        HRESULT hr = m_d3dDevice->CreateBuffer(&mpBufferDesc, &mpInitData,
            m_ssaoMPBuffer.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    {
        // Create SSAO_PARAMETER_BUFFER buffer.
        SSAO_PARAMETER_BUFFER mpBufferData;
        mpBufferData.radius = m_ssaoRadius;
        mpBufferData.bias = m_ssaoBias;
        mpBufferData.kernelSize = m_ssaoKernelSize;
        mpBufferData.useSSAO = m_useSSAO;

        // Fill in a buffer description.
        D3D11_BUFFER_DESC mpBufferDesc;
        mpBufferDesc.ByteWidth = sizeof(SSAO_PARAMETER_BUFFER);
        mpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        mpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        mpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        mpBufferDesc.MiscFlags = 0;
        mpBufferDesc.StructureByteStride = 0;

        // Fill in the subresource data.
        D3D11_SUBRESOURCE_DATA mpInitData;
        mpInitData.pSysMem = &mpBufferData;
        mpInitData.SysMemPitch = 0;
        mpInitData.SysMemSlicePitch = 0;

        // Create the buffer.
        HRESULT hr = m_d3dDevice->CreateBuffer(&mpBufferDesc, &mpInitData,
            m_ssaoParameterBuffer.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    // Create the main occlusion texture and the blurred version texture.
    initSSAOOccMaps();
}


/*
 * SponzaScene::initSSAOOccMaps
 */
void SponzaScene::initSSAOOccMaps() {
    {
        // Create the occlusion texture.
        D3D11_TEXTURE2D_DESC occlusionTextureDesc = {};
        occlusionTextureDesc.Width = m_wWidth;      // Replace with your screen width.
        occlusionTextureDesc.Height = m_wHeight;    // Replace with your screen height.
        occlusionTextureDesc.MipLevels = 1;
        occlusionTextureDesc.ArraySize = 1;
        occlusionTextureDesc.Format = DXGI_FORMAT_R8_UNORM; // Equivalent to GL_RED in OpenGL.
        occlusionTextureDesc.SampleDesc.Count = 1;
        occlusionTextureDesc.SampleDesc.Quality = 0;
        occlusionTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        occlusionTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Bind as render target and shader resource.
        occlusionTextureDesc.CPUAccessFlags = 0;
        occlusionTextureDesc.MiscFlags = 0;

        HRESULT hr = m_d3dDevice->CreateTexture2D(&occlusionTextureDesc, nullptr,
            m_occlusionTexture.GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a RENDERTARGET view on the texture.
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = occlusionTextureDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        hr = m_d3dDevice->CreateRenderTargetView(m_occlusionTexture.Get(),
            &rtvDesc, m_occlusionTextureRTV.GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a SHADER RESOURCE view on the texture.
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {}; // Fill in SRV description
        srvDesc.Format = occlusionTextureDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        hr = m_d3dDevice->CreateShaderResourceView(m_occlusionTexture.Get(),
            &srvDesc, m_occlusionTextureSRV.GetAddressOf());
        assert(SUCCEEDED(hr));
    }

    {
        // Create the blurred occlusion texture.
        D3D11_TEXTURE2D_DESC occlusionTextureDesc = {};
        occlusionTextureDesc.Width = m_wWidth;      // Replace with your screen width.
        occlusionTextureDesc.Height = m_wHeight;    // Replace with your screen height.
        occlusionTextureDesc.MipLevels = 1;
        occlusionTextureDesc.ArraySize = 1;
        occlusionTextureDesc.Format = DXGI_FORMAT_R8_UNORM; // Equivalent to GL_RED in OpenGL.
        occlusionTextureDesc.SampleDesc.Count = 1;
        occlusionTextureDesc.SampleDesc.Quality = 0;
        occlusionTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        occlusionTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Bind as render target and shader resource.
        occlusionTextureDesc.CPUAccessFlags = 0;
        occlusionTextureDesc.MiscFlags = 0;

        HRESULT hr = m_d3dDevice->CreateTexture2D(&occlusionTextureDesc, nullptr,
            m_occlusionTextureBlur.GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a RENDERTARGET view on the texture.
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = occlusionTextureDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        hr = m_d3dDevice->CreateRenderTargetView(m_occlusionTextureBlur.Get(),
            &rtvDesc, m_occlusionTextureBlurRTV.GetAddressOf());
        assert(SUCCEEDED(hr));

        // Create a SHADER RESOURCE view on the texture.
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {}; // Fill in SRV description
        srvDesc.Format = occlusionTextureDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        hr = m_d3dDevice->CreateShaderResourceView(m_occlusionTextureBlur.Get(),
            &srvDesc, m_occlusionTextureBlurSRV.GetAddressOf());
        assert(SUCCEEDED(hr));
    }
}


/*
 * SponzaScene::initProfiling
 */
void SponzaScene::initProfiling() {
    // Create queries for profiling.
    for (unsigned int i = 0; i < QUERY_BUFFER_CNT; i++) {
        // Basic queries.
        pQueryDisjoint[i] = createDisjointQuery(m_d3dDevice);
        pQueryBeginFrame[i] = createTimestampQuery(m_d3dDevice);
        pQueryEndFrame[i] = createTimestampQuery(m_d3dDevice);

        // Other queries.
        pQueryLightViewPass[i] = createTimestampQuery(m_d3dDevice);
        pGeometryPass[i] = createTimestampQuery(m_d3dDevice);
        pLightVolumePass[i] = createTimestampQuery(m_d3dDevice);
        pSSAO[i] = createTimestampQuery(m_d3dDevice);
        pCombinationPass[i] = createTimestampQuery(m_d3dDevice);
        pForwardPass[i] = createTimestampQuery(m_d3dDevice);
    }
}


/*
 * SponzaScene::processPerformanceMetrics
 */
void SponzaScene::processPerformanceMetrics() {
    // Compute past query index, that is being used for lookup now. We use the
        // oldest query.
    unsigned int currentLookupIdx = (m_currentQueryIdx + 1) % QUERY_BUFFER_CNT;

    // Wait for data to be available. Should work first try if QUERY_BUFFER_CNT
    // is big enough.
    while (m_d3dContext->GetData(
        pQueryDisjoint[currentLookupIdx].Get(), NULL, 0, 0) == S_FALSE) {
        Sleep(1);       // Wait a bit, but give other threads a chance to run
    }

    // Check whether timestamps were disjoint during the last frame.Should work
    // first try if QUERY_BUFFER_CNT is big enough.
    D3D10_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
    while (m_d3dContext->GetData(pQueryDisjoint[currentLookupIdx].Get(),
        &tsDisjoint, sizeof(tsDisjoint), 0) == S_FALSE) {
        Sleep(1);       // Wait a bit, but give other threads a chance to run
    }

    // Get all the timestamps. Should work first try if QUERY_BUFFER_CNT is
    // big enough.
    UINT64 tsBeginFrame;
    UINT64 tsEndFrame;

    UINT64 tsLightViewPass;
    UINT64 tsGeometryPass;
    UINT64 tsLightVolumePass;
    UINT64 tsSSAO;
    UINT64 tsCombinationPass;
    UINT64 tsForwardPass;

    while (m_d3dContext->GetData(pQueryBeginFrame[currentLookupIdx].Get(), &tsBeginFrame, sizeof(UINT64), 0) == S_FALSE) {
        Sleep(1);       // Wait a bit, but give other threads a chance to run
    }
    while (m_d3dContext->GetData(pQueryEndFrame[currentLookupIdx].Get(), &tsEndFrame, sizeof(UINT64), 0) == S_FALSE) {
        Sleep(1);       // Wait a bit, but give other threads a chance to run
    }


    while (m_d3dContext->GetData(pQueryLightViewPass[currentLookupIdx].Get(), &tsLightViewPass, sizeof(UINT64), 0) == S_FALSE) {
        Sleep(1);       // Wait a bit, but give other threads a chance to run
    }
    while (m_d3dContext->GetData(pGeometryPass[currentLookupIdx].Get(), &tsGeometryPass, sizeof(UINT64), 0) == S_FALSE) {
        Sleep(1);       // Wait a bit, but give other threads a chance to run
    }
    while (m_d3dContext->GetData(pLightVolumePass[currentLookupIdx].Get(), &tsLightVolumePass, sizeof(UINT64), 0) == S_FALSE) {
        Sleep(1);       // Wait a bit, but give other threads a chance to run
    }
    while (m_d3dContext->GetData(pSSAO[currentLookupIdx].Get(), &tsSSAO, sizeof(UINT64), 0) == S_FALSE) {
        Sleep(1);       // Wait a bit, but give other threads a chance to run
    }
    while (m_d3dContext->GetData(pCombinationPass[currentLookupIdx].Get(), &tsCombinationPass, sizeof(UINT64), 0) == S_FALSE) {
        Sleep(1);       // Wait a bit, but give other threads a chance to run
    }
    while (m_d3dContext->GetData(pForwardPass[currentLookupIdx].Get(), &tsForwardPass, sizeof(UINT64), 0) == S_FALSE) {
        Sleep(1);       // Wait a bit, but give other threads a chance to run
    }

    // Convert to real time (Milliseconds).
    float msFrameTime = float(tsEndFrame - tsBeginFrame) /
        float(tsDisjoint.Frequency) * 1000.0f;

    float msLightViewPass = float(tsLightViewPass - tsBeginFrame) /
        float(tsDisjoint.Frequency) * 1000.0f;
    float msGeometryPass = float(tsGeometryPass - tsBeginFrame) /
        float(tsDisjoint.Frequency) * 1000.0f;
    float msLightVolumePass = float(tsLightVolumePass - tsGeometryPass) /
        float(tsDisjoint.Frequency) * 1000.0f;
    float msSSAO = float(tsSSAO - tsLightVolumePass) /
        float(tsDisjoint.Frequency) * 1000.0f;
    float msCombinationPass = float(tsCombinationPass - tsSSAO) /
        float(tsDisjoint.Frequency) * 1000.0f;
    float msForwardPass = float(tsForwardPass - tsCombinationPass) /
        float(tsDisjoint.Frequency) * 1000.0f;

    // Every QUERY_BUFFER_CNT frames, the timings will be updated.
    if (m_currentQueryIdx == 0) {
        m_msFrameTime = msFrameTime;
        m_msLightViewPass = msLightViewPass;
        m_msGeometryPass = msGeometryPass;
        m_msLightVolumePass = msLightVolumePass;
        m_msSSAO = msSSAO;
        m_msCombinationPass = msCombinationPass;
        m_msForwardPass = msForwardPass;
    }

    // Next frame, another query collection will be used.
    m_currentQueryIdx = (m_currentQueryIdx + 1) % QUERY_BUFFER_CNT;
}
