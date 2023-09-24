#pragma once
#include "stdafx.h"
#include "Application.h"
#include "Helper.h"

/*
 * Application::Application
 */
Application::Application(int wWidth, int wHeight) : m_applicationName(L"Empty"),
    m_hwnd(0), m_hinstance(0), m_d3disReady(false), m_wWidth(wWidth),
    m_wHeight(wHeight) {/*empty*/ }


/*
 * Application::~Application
 */
Application::~Application(){}


/*
 * Application::Initialize
 */
void Application::Initialize() {
    // Init the Win32 resources.
    initWindows();

    // Create input managers.
    m_controls = std::make_shared<InputControls>();
    m_controls->m_keyboard = std::make_unique<DirectX::Keyboard>();
    m_mouse = std::make_unique<Mouse>();    // TODO: Move into m_controls.

    // Create d3dRenderer which contains all D3D11 related resources.
    m_d3dRenderer = std::make_unique<Graphics>(&m_hwnd, m_wWidth, m_wHeight,
        m_controls);
    m_d3disReady = true;    // TODO: Maybe remove in future.Used for inital WM_SIZE.
}


/*
 * Application::Run
 */
void Application::Run() {
    // Performance measurements.
    LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
    LARGE_INTEGER perfCounterFreq;
    QueryPerformanceFrequency(&perfCounterFreq);

    // Run the message loop.
    bool isRunning = true;
    MSG msg = { };
    int frameCnt = 0;
    while (isRunning) {
        // User input and window events. PeekMessage() is non-blocking, i.e we are
        // not waiting for an user input (unlike GetMessage()).
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);

            // Each time the program uses DispatchMessage, it indirectly causes 
            // Windows to invoke the WindowProc function, once for each message.
            DispatchMessage(&msg);
        }

        if (msg.message == WM_QUIT) {
            isRunning = false;
            /*** TODO: CLEAN UP ***/
            break;
        }

        // Start performance measure.
        QueryPerformanceCounter(&startingTime);

        // Rendering of frames:
        m_d3dRenderer->RenderFrame();

        // Total frame time in micro seconds.
        QueryPerformanceCounter(&endingTime);
        elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
        elapsedMicroseconds.QuadPart *= 1000000;
        elapsedMicroseconds.QuadPart /= perfCounterFreq.QuadPart;

        // Get mouse position for display.
		int mouseX = m_mouse->GetPosX();
		int mouseY = m_mouse->GetPosY();

        // Output frames/seconds for every 1000th frame.
        if ((frameCnt % 100)) {
            int fps = 1000000.0 / elapsedMicroseconds.QuadPart;
            std::string updatedTitle = m_windowTitle
                + " ["
                + std::to_string(fps)
                + " fps]"
                + "  Mouse: ("
                + std::to_string(mouseX)
                + " : "
                + std::to_string(mouseY) + ")"
                + "   " + std::to_string(m_mouse->LeftIsPressed())
                + "   " + std::to_string(m_mouse->RightIsPressed())
                + "   " + std::to_string(m_mouse->GetWheelValue());
            SetWindowText(m_hwnd, Helper::ConvertUtf8ToWide(updatedTitle).c_str());
        }
        ++frameCnt;
    }
}


/// <summary>
/// ImGui WindowProc handler which applies user input to ImGui elements.
/// </summary>
/// <param name="hwnd"></param>
/// <param name="uMsg"></param>
/// <param name="wParam"></param>
/// <param name="lParam"></param>
/// <returns></returns>
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam);


/*
 * Application::MessageHandler
 */
LRESULT Application::MessageHandler(HWND hwnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam) {
    // Apply changes coming from messages.
    switch (uMsg) {
        case WM_ACTIVATEAPP:
        {
            DirectX::Keyboard::ProcessMessage(uMsg, wParam, lParam);
            break;
        }

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            DirectX::Keyboard::ProcessMessage(uMsg, wParam, lParam);
            break;
        }

        case WM_SYSKEYDOWN:
        {
            DirectX::Keyboard::ProcessMessage(uMsg, wParam, lParam);
            if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000) {
                //...
            }
            break;
        }
            
        case WM_SIZE:
        {
            // If all D3D11 resources are already initialized, resize framebuffer.
            if (m_d3disReady) {
                m_d3dRenderer->ResizeWindow(m_wWidth, m_wHeight);
            }
            return 0;
            break;
        }

		case WM_MOUSEMOVE:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            if (pt.x >= 0 && pt.x < m_wWidth && pt.y >= 0 && pt.y < m_wHeight) {
                m_mouse->OnMouseMove(pt.x, pt.y);
                if (!m_mouse->IsInWindow()) {
                    SetCapture(m_hwnd);
                    m_mouse->OnMouseEnter();
                }
            } else {
                if (wParam & (MK_LBUTTON | MK_RBUTTON)) {
                    m_mouse->OnMouseMove(pt.x, pt.y);
                } else {
                    ReleaseCapture();
                    m_mouse->OnMouseLeave();
                }
            }
            return 0;
            break;
        }
			
		case WM_LBUTTONDOWN:
        {
            SetForegroundWindow(m_hwnd);
            const POINTS pt = MAKEPOINTS(lParam);
            m_mouse->OnLeftPressed();
            return 0;
            break;
        }
			
		case WM_RBUTTONDOWN:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            m_mouse->OnRightPressed();
            return 0;
            break;
        }
			
		case WM_LBUTTONUP:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            m_mouse->OnLeftReleased();
            // release mouse if outside of window
            if (pt.x < 0 || pt.x >= m_wWidth || pt.y < 0 || pt.y >= m_wHeight) {
                ReleaseCapture();
                m_mouse->OnMouseLeave();
            }
            return 0;
            break;
        }
			
		case WM_RBUTTONUP:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            m_mouse->OnRightReleased();
            // release mouse if outside of window
            if (pt.x < 0 || pt.x >= m_wWidth || pt.y < 0 || pt.y >= m_wHeight) {
                ReleaseCapture();
                m_mouse->OnMouseLeave();
            }
            return 0;
            break;
        }

		case WM_MOUSEWHEEL:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            m_mouse->OnWheelValue(delta);
            return 0;
            break;
        }

        // Default window procedure to provide default processing for any window
        // messages that the application does not process.
        default:
        {
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        } 
    }
}


/*
 * Application::initWindows
 */
void Application::initWindows() {
    //Get an external pointer to this object and set as global.
    g_applicationHandle = this;

    // Get the instance of this application.
    m_hinstance = GetModuleHandle(NULL);

    // Set application name and window title.
    m_applicationName = L"D3D11 Project";
    m_windowTitle = "D3D11 Project";

    // Register the window class.
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;    // Set the (default) Windwos Message handler.
    wc.hInstance = m_hinstance;     // we use our member hinstance.
    wc.lpszClassName = m_applicationName;
    RegisterClass(&wc);

    // Create the window. CreateWindowEx returns handle to the new window.
    m_hwnd = CreateWindowEx(
        0,                                                  // Opt. window styles.
        m_applicationName,                                  // Window class.
        Helper::ConvertUtf8ToWide(m_windowTitle).c_str(),   // Window text.
        WS_OVERLAPPEDWINDOW,                                // Window style.

        // Position and size.
        CW_USEDEFAULT, CW_USEDEFAULT, m_wWidth, m_wHeight,

        0,              // Parent window. 
        0,              // Menu.
        m_hinstance,    // We use our member hinstance.
        0               // Additional application data.
    );
    assert(m_hwnd);

    // Bring the window up on the screen and set it as main focus.
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);
}

/// <summary>
/// Process window messages.
/// </summary>
/// <param name="hwnd"></param>
/// <param name="uMsg"></param>
/// <param name="wParam"></param>
/// <param name="lParam"></param>
/// <returns></returns>
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // First relay messages to special ImGui window proc handler to apply changes to
    // ImGui elements.
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) {
        return true;
    }

    // Apply changes coming from messages.
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
        break;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
        break;
    }

    // Other messages will be relayed to our own message handler.
    return g_applicationHandle->MessageHandler(hwnd, uMsg, wParam, lParam);
}