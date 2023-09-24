#pragma once
#include "Graphics.h"
#include "Mouse.h"
#include "Helper.h"

/// <summary>
/// Class which performs the the basic Win32 application loop and message handling.
/// </summary>
class Application {
public:
	/// <summary>
	/// Constructor.
	/// </summary>
	/// <param name="wWidth">Window width.</param>
	/// <param name="wHeight">Window height.</param>
	Application(int wWidth, int wHeight);

	~Application();

	/// <summary>
	/// Inits the Win32 application window and D3D11 (m_d3dRenderer).
	/// </summary>
	void Initialize();

	/// <summary>
	/// Win32 application loop.
	/// </summary>
	void Run();

	/// <summary>
	/// Our message handler which relays messages to ImGui and extracts keyboard
	/// inputs etc.
	/// </summary>
	/// <param name=""></param>
	/// <param name=""></param>
	/// <param name=""></param>
	/// <param name=""></param>
	/// <returns></returns>
	LRESULT CALLBACK MessageHandler(HWND, UINT, WPARAM, LPARAM);

private:
	/// <summary>
	/// Creates a Win32 application window.
	/// </summary>
	/// <remarks>
	/// The global g_applicationHandle contains a pointer to the single Application
	/// object we use throughout the whole application. We do this so we can create
	/// our own MessageHandler which will receive messages from the normal
	/// WindowProc callback-function. We want our own MessageHandler because this
	/// one, unlike the static WindowProc by Windows, has access to our private
	/// d3dRenderer. As a result is everything nicely encapsulated and we can access
	/// our encapsulted D3D11 stuff. Inspired by
	/// https://devblogs.microsoft.com/oldnewthing/20140203-00/?p=1893 and 
	/// http://www.rastertek.com/dx11tut02.html .
	/// </remarks>
	void initWindows();

	// Win32 resources.
	LPCWSTR m_applicationName;	// Wide string of application name.
	std::string m_windowTitle;	// String of window title.
	HINSTANCE m_hinstance;		// Instance handle.
	HWND m_hwnd;				// Window handle.
	int m_wWidth;
	int m_wHeight;

	std::unique_ptr<Graphics> m_d3dRenderer;	// D3D11 related resources.
	bool m_d3disReady;	// TODO: Maybe remove in future. Used for initial WM_SIZE.

	// Input.
	std::unique_ptr <Mouse> m_mouse;	// TODO: Move into InputControls
	std::shared_ptr <InputControls> m_controls;
};

/// <summary>
/// Invoked by Windows after we called DispatchMessage(&msg) for each incoming msg.
/// </summary>
/// <param name="hwnd"></param>
/// <param name="uMsg"></param>
/// <param name="wParam"></param>
/// <param name="lParam"></param>
/// <returns></returns>
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam);

// Global pointer to single Application instance.
static Application* g_applicationHandle = 0;
