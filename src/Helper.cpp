#pragma once

#include "stdafx.h"
#include "Helper.h"

/*
 * Helper::ConvertUtf8ToWide
 */
std::wstring Helper::ConvertUtf8ToWide(const std::string& str){
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0);
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &wstr[0], count);
    return wstr;
}


/*
 * Helper::GetCurrentPathWstring
 */
std::wstring Helper::GetCurrentPathWstring(){
    //std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return ConvertUtf8ToWide(std::filesystem::current_path().string());
}


/*
 * Helper::GetCurrentPathString
 */
std::string Helper::GetCurrentPathString() {
    return std::filesystem::current_path().string();
}


/*
 * Helper::GetAssetFullPath
 */
std::wstring Helper::GetAssetFullPath(LPCWSTR assetName){
    return GetCurrentPathWstring() + assetName;
}


/*
 * Helper::GetAssetFullPath
 */
std::string Helper::GetAssetFullPathString(std::string assetName) {
    return GetCurrentPathString() + assetName;
}


/*
 * Helper::CreateVertexShader
 */
bool Helper::CreateVertexShader(LPCWSTR path, wrl::ComPtr<ID3DBlob>& byteCodePtr,
        wrl::ComPtr<ID3D11VertexShader>& vertexShaderTarget,
        wrl::ComPtr<ID3D11Device>& d3dDevice) {
#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    // Compile the shader.
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompileFromFile(
        Helper::GetAssetFullPath(path).c_str(),
        nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", compileFlags,
        0, byteCodePtr.GetAddressOf(), &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        assert(false);
    }

    // Create Vertex shader.
    assert(d3dDevice);
    hr = d3dDevice->CreateVertexShader(byteCodePtr->GetBufferPointer(),
        byteCodePtr->GetBufferSize(), 0, vertexShaderTarget.GetAddressOf());
    assert(SUCCEEDED(hr));

    return SUCCEEDED(hr);
}


/*
 * Helper::CreatePixelShader
 */
bool Helper::CreatePixelShader(LPCWSTR path, wrl::ComPtr<ID3DBlob>& byteCodePtr,
        wrl::ComPtr<ID3D11PixelShader>& pixelShaderTarget,
        wrl::ComPtr<ID3D11Device>& d3dDevice) {
#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    // Compile the pixel shader.
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompileFromFile(
        Helper::GetAssetFullPath(path).c_str(), nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", compileFlags,
        0, byteCodePtr.GetAddressOf(), &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        assert(false);
    }

    // Create Pixel shader.
    assert(d3dDevice);
    hr = d3dDevice->CreatePixelShader(byteCodePtr->GetBufferPointer(),
        byteCodePtr->GetBufferSize(), 0, pixelShaderTarget.GetAddressOf());
    assert(SUCCEEDED(hr));

    return SUCCEEDED(hr);
}


DWORD Win32FromHResult(HRESULT hr) {
    if ((hr & 0xFFFF0000) == MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, 0))
    {
        return HRESULT_CODE(hr);
    }

    if (hr == S_OK)
    {
        return ERROR_SUCCESS;
    }

    // Not a Win32 HRESULT so return a generic error code.
    return ERROR_CAN_NOT_COMPLETE;
}