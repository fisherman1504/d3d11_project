#include "stdafx.h"
#include "Application.h"

/// <summary>
/// Entry point of a Win32 application. Taken from
/// https://docs.microsoft.com/en-us/windows/win32/learnwin32/
/// your-first-windows-program
/// </summary>
/// <remarks>
/// Win32 programs no longer have a console for printed output. This means printf()
/// and cout output won't appear anywhere. Instead call OutputDebugStringA() to
/// print a string.
/// </remarks>
/// <param name="hInstance"></param>
/// <param name="hPrevInstance"></param>
/// <param name="pCmdLine"></param>
/// <param name="nCmdShow"></param>
/// <returns>Status code of application.</returns>
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
        _In_ PWSTR pCmdLine, _In_ int nCmdShow){
    // Create an application which performs the basic Win32 application loop and
    // message handling. This application contains the d3dRenderer for D3D11 stuff.
    std::unique_ptr<Application> app = std::make_unique<Application>(1400,800);
    app->Initialize();
    app->Run();
    return 0;
}