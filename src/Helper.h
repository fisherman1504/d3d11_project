#pragma once
#include <DirectXMath.h>
#include <Keyboard.h>

#define D3DCOLOR(r,g,b,a) r,g,b,a

struct InputControls {
	std::unique_ptr<DirectX::Keyboard> m_keyboard;
};

/// <summary>
/// Helper class for various needs.
/// </summary>
class Helper{
public:
	/// <summary>
	/// Converts an std::string to a std::wstring using windows.h.
	/// </summary>
	/// From user stax76:
	/// https://stackoverflow.com/questions/6693010/
	/// how-do-i-use-multibytetowidechar/6693107
	/// <param name="str">String which should be converted.</param>
	/// <returns>Converted wstring result.</returns>
	static std::wstring ConvertUtf8ToWide(const std::string& str);

	/// <summary>
	/// Returns the root working directory.
	/// </summary>
	/// <returns>Root working directory as a wstring.</returns>
	static std::wstring GetCurrentPathWstring();

	/// <summary>
	/// Returns the root working directory.
	/// </summary>
	/// <returns>Root working directory as a string.</returns>
	static std::string GetCurrentPathString();

	/// <summary>
	/// Appends the working directory path to the beginning of input.
	/// </summary>
	/// <param name="assetName">Name of the asset.</param>
	/// <returns>Full path to asset as a wstring.</returns>
	static std::wstring GetAssetFullPath(LPCWSTR assetName);

	/// <summary>
	/// Appends the working directory path to the beginning of input.
	/// </summary>
	/// <param name="assetName">Name of the asset.</param>
	/// <returns>Full path to asset as a string.</returns>
	static std::string GetAssetFullPathString(std::string assetName);

	/// <summary>
	/// Creates a vertex shader.
	/// </summary>
	/// <param name="path">Path to the .hlsl file.</param>
	/// <param name="byteCodePtr">Ptr to byte code of shader.</param>
	/// <param name="vertexShaderTarget">Ptr to shader.</param>
	/// <param name="d3dDevice">D3D11 device in use.</param>
	/// <returns>If creation was successful or not.</returns>
	static bool CreateVertexShader(LPCWSTR path,
		wrl::ComPtr<ID3DBlob>& byteCodePtr,
		wrl::ComPtr<ID3D11VertexShader>& vertexShaderTarget,
		wrl::ComPtr<ID3D11Device>& d3dDevice);

	/// <summary>
	/// Creates a pixel shader.
	/// </summary>
	/// <param name="path">Path to the .hlsl file.</param>
	/// <param name="byteCodePtr">Ptr to byte code of shader.</param>
	/// <param name="pixelShaderTarget">Ptr to shader.</param>
	/// <param name="d3dDevice">D3D11 device in use.</param>
	/// <returns>If creation was successful or not.</returns>
	static bool CreatePixelShader(LPCWSTR path,
		wrl::ComPtr<ID3DBlob>& byteCodePtr,
		wrl::ComPtr<ID3D11PixelShader>& pixelShaderTarget,
		wrl::ComPtr<ID3D11Device>& d3dDevice);
};


// https://stackoverflow.com/questions/22233527/how-to-convert-hresult-into-an-error-description
DWORD Win32FromHResult(HRESULT hr);