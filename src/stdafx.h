// Pre-compiled header.
#pragma once

#ifndef UNICODE
#define UNICODE
#endif 

// Windows stuff.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN	// Exclude rarely-used stuff from Windows headers.
#endif
#define NOMINMAX
#include <Windows.h>

// DirectX 11 specific headers.
#include <d3d11.h>
#include <dxgi.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>

// Math.
#include <DirectXMath.h>
#include "SimpleMath.h"
#include <Model.h>
#include <DirectXMesh.h>
namespace dx = DirectX;
namespace sm = DirectX::SimpleMath;

// File IO.
// Load 2D bitmaps (BMP, JPEG, PNG, TIFF, GIF,...).
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>


// 3D model loader.
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// For .dds files (1D, 2D, volume maps, cubemaps, mipmap levels, texture arrays,
// cubemap arrays, Block Compressed formats, etc.):
// https://github.com/microsoft/DirectXTK/wiki/DDSTextureLoader
#include <DDSTextureLoader.h>


// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
namespace wrl = Microsoft::WRL;

// STL Headers.
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <tchar.h>
#include <queue>
#include <array>
#include <thread>
#include <iostream>
#include <locale>
#include <codecvt>
#include <random>