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

// Два браузера: основной и header (OSR)
CefRefPtr<CefBrowser> g_main_browser;

// Два handler'а — по одному на браузер, чтобы в OnAfterCreated можно понять, для какого браузера коллбек
CefRefPtr<class SimpleHandler> g_main_handler;

HWND g_hwnd = nullptr;

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// Прототип функции modifiers (надо объявить до использования в WndProc)
uint32_t GetCefStateModifiers(WPARAM wparam);

std::atomic<bool> g_is_closing = false;

// Класс для обработки рендеринга и drag
#include <mutex>
#include <atomic>

class SimpleHandler : public CefClient,
    public CefLifeSpanHandler,
    public CefDragHandler
{
public:
    SimpleHandler() = default;

    // --- ДОБАВИТЬ ЭТИ ПЕРЕМЕННЫЕ ---
    int render_width = 0;
    int render_height = 0;

    // CefClient: возвращаем handlers
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }
    CefRefPtr<CefDragHandler> GetDragHandler() override {
        return this;
    }
    // --- ДОБАВИТЬ ЭТОТ МЕТОД ---
    // CefClient: возвращаем RenderHandler ТОЛЬКО для OSR браузера

    // CefDragHandler: получаем drag-регионы из рендерера

    bool DoClose(CefRefPtr<CefBrowser> browser) override {
        // не форсируем закрытие здесь
        return false;
    }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        if (!g_main_browser) {
            g_main_browser = browser;
        }
    }


    // ... (остальной код без изменений)

    // В классе SimpleHandler, в OnBeforeClose:
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        // Сбрасываем соответствующую глобальную ссылку
        if (this == g_main_handler.get()) {
            g_main_browser = nullptr;
        }

        // Если все браузеры закрыты (в вашем случае только один), выходим из message loop
        if (!g_main_browser) {
            // Опционально: DestroyWindow(g_hwnd); // Если хотите явно закрыть окно перед выходом
            CefQuitMessageLoop();
        }
    }

private:
    IMPLEMENT_REFCOUNTING(SimpleHandler);
};

// создаём handler'ы глобально (инициализируем ниже в main)
CefRefPtr<SimpleHandler> init_main_handler() { return new SimpleHandler(); }
CefRefPtr<SimpleHandler> init_header_handler() { return new SimpleHandler(); }

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
    settings.windowless_rendering_enabled = false; // по-умолчанию off; будем включать для header при создании окна

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

    // Инициализируем handler'ы
    g_main_handler = init_main_handler();

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
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);

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

    int width = 1452;
    int height = 879;

    HWND hWnd = CreateWindowW(
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,  // <-- стандартное окно с заголовком
        CW_USEDEFAULT, 0, width, height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd)
        return FALSE;

    g_hwnd = hWnd;

    RECT rc;

    GetClientRect(hWnd, &rc);


    // --- Основное окно CEF (без OSR) ---
    CefWindowInfo main_info;
    main_info.SetAsChild(hWnd, CefRect(0, 0, rc.right - rc.left, rc.bottom - rc.top));
    // windowless_rendering_enabled по умолчанию false

    CefBrowserSettings main_settings;
    CefBrowserHost::CreateBrowser(
        main_info,
        g_main_handler,
        L"http://localhost:5173/", // ваш URL
        main_settings,
        nullptr,
        nullptr
    );

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

// helper: возвращает host по координате y (client coords)
inline CefRefPtr<CefBrowserHost> HostForY(int y) {
    if (g_main_browser) return g_main_browser->GetHost();
    return nullptr;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        if (g_main_browser) {
            CefRefPtr<CefBrowserHost> host = g_main_browser->GetHost();
            if (host) {
                HWND bh = (HWND)host->GetWindowHandle();
                if (bh) {
                    MoveWindow(bh, 0, 0, width, height, TRUE);
                }
                host->WasResized();
            }
        }
        return 0;
    }


    case WM_ERASEBKGND:
        return 1;  // Не стираем фон, т.к. рендерим сами

    case WM_CLOSE:
        if (g_is_closing) {
            return 0;  // Игнорируем повторные клики
        }
        g_is_closing = true;

        // Закрываем браузер(ы) корректно
        if (g_main_browser) {
            g_main_browser->GetHost()->CloseBrowser(false);  // Gentle close
            return 0;  // Не разрушаем окно сразу
        }

        // Если браузера уже нет, разрушаем окно
        DestroyWindow(hWnd);
        return 0;

        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

        // Мышь: маршрутизируем в header или main по Y
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

        CefRefPtr<CefBrowserHost> host = HostForY(y);
        if (!host) return 0;

        CefMouseEvent event;
        event.x = x;
        event.modifiers = GetCefStateModifiers(wParam);

        if (message == WM_MOUSEWHEEL) {
            POINT pt = { x, y };
            ScreenToClient(hWnd, &pt);
            event.x = pt.x;
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            host->SendMouseWheelEvent(event, 0, delta);
        }
        else if (message == WM_MOUSEMOVE) {
            // for OSR header we send mouse move; for windowed main we also send (CEF will handle)
            host->SendMouseMoveEvent(event, false);
        }
        else {
            CefBrowserHost::MouseButtonType btn = static_cast<CefBrowserHost::MouseButtonType>(0); // left
            if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) btn = static_cast<CefBrowserHost::MouseButtonType>(1);
            else if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) btn = static_cast<CefBrowserHost::MouseButtonType>(2);

            bool mouse_up = (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP);
            int click_count = (message == WM_LBUTTONDBLCLK ? 2 : 1);
            host->SendMouseClickEvent(event, btn, mouse_up, click_count);
        }
        return 0;
    }

                      // Клавиши — отправляем в основной браузер (можно менять логику при необходимости)
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR: {
        if (!g_main_browser) return 0;
        CefRefPtr<CefBrowserHost> host = g_main_browser->GetHost();
        if (!host) return 0;

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
        if (g_main_browser) g_main_browser->GetHost()->SetFocus(message == WM_SETFOCUS);
        return 0;
    }

    case WM_NCHITTEST: {
        // Для frameless/resizable окна поддерживаем resize, draggable regions в header и HTCAPTION там
        LRESULT result = DefWindowProc(hWnd, message, wParam, lParam);

        if (result == HTCLIENT) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hWnd, &pt);

            // Проверяем края для resize (как раньше)
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
