// CEF.cpp : Определяет точку входа для приложения.
//

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Windowsx.h>

#pragma push_macro("GetNextSibling")
#pragma push_macro("GetFirstChild")
#pragma push_macro("GetParent")
#pragma push_macro("GetLastChild")
#pragma push_macro("GetClassName")
#pragma push_macro("SetParent")
#pragma push_macro("GetObject")
#pragma push_macro("GetWindowText")

#ifdef GetNextSibling
#undef GetNextSibling
#endif
#ifdef GetFirstChild
#undef GetFirstChild
#endif
#ifdef GetParent
#undef GetParent
#endif
#ifdef GetLastChild
#undef GetLastChild
#endif
#ifdef GetClassName
#undef GetClassName
#endif
#ifdef SetParent
#undef SetParent
#endif
#ifdef GetObject
#undef GetObject
#endif
#ifdef GetWindowText
#undef GetWindowText
#endif

#include <vector>
#include <cstdint>

#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_browser.h"
#include "include/cef_render_handler.h"
#include "include/cef_drag_handler.h"
#include "include/base/cef_scoped_refptr.h"
#include "include/internal/cef_types_wrappers.h"
#include "include/cef_base.h"

#pragma pop_macro("GetWindowText")
#pragma pop_macro("GetObject")
#pragma pop_macro("SetParent")
#pragma pop_macro("GetClassName")
#pragma pop_macro("GetLastChild")
#pragma pop_macro("GetParent")
#pragma pop_macro("GetFirstChild")
#pragma pop_macro("GetNextSibling")
#include "resource.h"
#include "Api.h"
#include "JsBridgeApp.h"
#include <mutex>
#include <atomic>
#include <d2d1.h>
#pragma comment(lib, "d2d1.lib")

#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна
CefRefPtr<CefBrowser> g_browser;
HWND g_hwnd = nullptr;
std::vector<CefDraggableRegion> g_draggable_regions;
std::mutex g_draggable_mutex;
static ID2D1HwndRenderTarget* renderTarget;


// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// Прототип функции modifiers (надо объявить до использования в WndProc)
uint32_t GetCefStateModifiers(WPARAM wparam);

// Класс для обработки рендеринга и drag
#include <mutex>
#include <atomic>

class SimpleHandler : public CefClient,
    public CefLifeSpanHandler,
    public CefDragHandler,
    public CefRenderHandler {  // <-- Добавьте это
public:
    // CefClient: возвращаем handlers
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }
    CefRefPtr<CefDragHandler> GetDragHandler() override {
        return this;
    }

    // CefClient: добавьте render handler
    CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        return this;
    }

    // CefRenderHandler: обязательные методы
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
        RECT clientRect;
        GetClientRect(g_hwnd, &clientRect);
        rect = CefRect(0, 0, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
    }

    void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects,
        const void* buffer, int width, int height) override {
        // Инициализация D2D (один раз)
        static ID2D1Factory* d2dFactory = nullptr;
        static ID2D1HwndRenderTarget* renderTarget = nullptr;
        if (!d2dFactory) {
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
        }
        if (!renderTarget) {
            RECT rc;
            GetClientRect(g_hwnd, &rc);
            D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
            D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
            rtProps.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
            d2dFactory->CreateHwndRenderTarget(rtProps, D2D1::HwndRenderTargetProperties(g_hwnd, size), &renderTarget);
        }

        // Resize target если размер изменился
        RECT rc;
        GetClientRect(g_hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        renderTarget->Resize(size);

        // Рендеринг буфера CEF в D2D bitmap
        renderTarget->BeginDraw();
        ID2D1Bitmap* bitmap = nullptr;
        D2D1_BITMAP_PROPERTIES bitmapProps = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        D2D1_SIZE_U bitmapSize = D2D1::SizeU(width, height);
        renderTarget->CreateBitmap(bitmapSize, buffer, width * 4, bitmapProps, &bitmap);  // buffer — ARGB от CEF (stride = width*4)

        // Отрисовка всего буфера (или по dirtyRects для оптимизации)
        for (const auto& dirty : dirtyRects) {
            D2D1_RECT_F destRect = D2D1::RectF(dirty.x, dirty.y, dirty.x + dirty.width, dirty.y + dirty.height);
            D2D1_RECT_F srcRect = destRect;  // 1:1
            renderTarget->DrawBitmap(bitmap, destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, srcRect);
        }

        renderTarget->EndDraw();
        bitmap->Release();  // Освободите ресурсы
    }

    // CefDragHandler: получаем drag-регионы из рендерера
    void OnDraggableRegionsChanged(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const std::vector<CefDraggableRegion>& regions) override {
            {
                std::lock_guard<std::mutex> lock(g_draggable_mutex);
                g_draggable_regions = regions;
            }
            // Отладочная информация — убедитесь, что колбэк вызывается
            OutputDebugStringA("OnDraggableRegionsChanged called\n");
    }

    // CefLifeSpanHandler
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        g_browser = browser;
    }

    bool DoClose(CefRefPtr<CefBrowser> browser) override {
        return false;
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        g_browser = nullptr;
    }

private:
    IMPLEMENT_REFCOUNTING(SimpleHandler);
};



CefRefPtr<SimpleHandler> g_handler(new SimpleHandler());

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Инициализация CEF
    CefMainArgs main_args(hInstance);
    CefSettings settings;
    settings.windowless_rendering_enabled = true;  // <-- Включите OSR

    CefRefPtr<CefApp> app = new JsBridgeApp();
    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    CefInitialize(main_args, settings, app, nullptr);

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CEF, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    // Основной цикл сообщений CEF
    CefRunMessageLoop();

    CefShutdown();
    return 0;
}


ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CEF));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    int width = 800;
    int height = 600;

    HWND hWnd = CreateWindowW(
        szWindowClass,
        szTitle,
        WS_POPUP | WS_VISIBLE | WS_THICKFRAME,  // WS_THICKFRAME для resize
        100, 100, width, height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd)
        return FALSE;

    g_hwnd = hWnd;
    CefWindowInfo window_info;
    window_info.SetAsWindowless(hWnd);  // <-- Для OSR, без дочернего окна

    CefBrowserSettings browser_settings;
    RECT rc;
    GetClientRect(hWnd, &rc);
    CefRect bounds(0, 0, rc.right - rc.left, rc.bottom - rc.top);
    window_info.SetAsChild(hWnd, bounds);


    CefBrowserHost::CreateBrowser(window_info, g_handler,
        L"http://localhost:5173/",
        browser_settings, nullptr, nullptr);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_browser) {
        CefRefPtr<CefBrowserHost> host = g_browser->GetHost();

        switch (message) {
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 200;  // Мин. ширина
            mmi->ptMinTrackSize.y = 200;  // Мин. высота
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            if (g_browser) g_browser->GetHost()->Invalidate(PET_VIEW);  // Запрос перерисовки
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (renderTarget) {
                D2D1_SIZE_U size = D2D1::SizeU(width, height);
                renderTarget->Resize(size);
            }
            if (g_browser) {
                g_browser->GetHost()->WasResized();
            }
            return 0;
        }


        case WM_ERASEBKGND:
            return 1;  // Не стираем фон, т.к. рендерим сами

        case WM_CLOSE:
            host->CloseBrowser(false);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            CefMouseEvent event;
            event.x = x;
            event.y = y;
            event.modifiers = GetCefStateModifiers(wParam);

            if (message == WM_MOUSEWHEEL) {
                POINT pt = { x, y };
                ScreenToClient(hWnd, &pt);
                event.x = pt.x;
                event.y = pt.y;
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                // wheelX = 0, wheelY = delta
                host->SendMouseWheelEvent(event, 0, delta);
            }
            else if (message == WM_MOUSEMOVE) {
                host->SendMouseMoveEvent(event, false);
            }
            else {
                // В некоторых версиях CEF имена перечислений MBT_* не находятся как члены CefBrowserHost,
                // поэтому приводим числовое значение к типу MouseButtonType.
                CefBrowserHost::MouseButtonType btn = static_cast<CefBrowserHost::MouseButtonType>(0); // left by default
                if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP)
                    btn = static_cast<CefBrowserHost::MouseButtonType>(1); // right
                else if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP)
                    btn = static_cast<CefBrowserHost::MouseButtonType>(2); // middle

                bool mouse_up = (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP);
                int click_count = (message == WM_LBUTTONDBLCLK ? 2 : 1);
                host->SendMouseClickEvent(event, btn, mouse_up, click_count);
            }
            return 0;
        }

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_SYSCHAR: {
            CefKeyEvent event;
            event.windows_key_code = (int)wParam;
            event.native_key_code = (int)lParam;
            event.modifiers = GetCefStateModifiers(wParam);

            if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
                event.type = KEYEVENT_RAWKEYDOWN;
            else if (message == WM_KEYUP || message == WM_SYSKEYUP)
                event.type = KEYEVENT_KEYUP;
            else
                event.type = KEYEVENT_CHAR;

            host->SendKeyEvent(event);
            return 0;
        }

        case WM_SETFOCUS:
        case WM_KILLFOCUS: {
            host->SetFocus(message == WM_SETFOCUS);
            return 0;
        }


        case WM_NCHITTEST: {
            LRESULT result = DefWindowProc(hWnd, message, wParam, lParam);
            if (result == HTCLIENT) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hWnd, &pt);

                // Проверяем draggable regions из CEF (под мьютексом)
                bool is_draggable = false;
                {
                    std::lock_guard<std::mutex> lock(g_draggable_mutex);
                    for (const auto& region : g_draggable_regions) {
                        const CefRect& b = region.bounds;
                        if (region.draggable) {
                            if (pt.x >= b.x && pt.x < b.x + b.width &&
                                pt.y >= b.y && pt.y < b.y + b.height) {
                                is_draggable = true;
                                break;
                            }
                        }
                    }
                }
                if (is_draggable) return HTCAPTION;


                // Проверяем края для resize
                RECT rc;
                GetClientRect(hWnd, &rc);
                int borderWidth = 10;

                bool left = pt.x < borderWidth;
                bool right = pt.x > rc.right - borderWidth;
                bool top = pt.y < borderWidth;
                bool bottom = pt.y > rc.bottom - borderWidth;

                if (top && left)      return HTTOPLEFT;
                if (top && right)     return HTTOPRIGHT;
                if (bottom && left)   return HTBOTTOMLEFT;
                if (bottom && right)  return HTBOTTOMRIGHT;
                if (top)              return HTTOP;
                if (bottom)           return HTBOTTOM;
                if (left)             return HTLEFT;
                if (right)            return HTRIGHT;
            }
            return result;
        }
        }
    }

    switch (message) {
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }
    case WM_DESTROY:
        CefShutdown();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Вспомогательная функция для modifiers
uint32_t GetCefStateModifiers(WPARAM wparam) {
    uint32_t modifiers = 0;
    if (wparam & MK_CONTROL)
        modifiers |= EVENTFLAG_CONTROL_DOWN;
    if (wparam & MK_SHIFT)
        modifiers |= EVENTFLAG_SHIFT_DOWN;
    if (GetKeyState(VK_MENU) < 0)
        modifiers |= EVENTFLAG_ALT_DOWN;
    if (wparam & MK_LBUTTON)
        modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    if (wparam & MK_MBUTTON)
        modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    if (wparam & MK_RBUTTON)
        modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    if (::GetKeyState(VK_CAPITAL) & 1)
        modifiers |= EVENTFLAG_CAPS_LOCK_ON;
    if (::GetKeyState(VK_NUMLOCK) & 1)
        modifiers |= EVENTFLAG_NUM_LOCK_ON;
    return modifiers;
}
