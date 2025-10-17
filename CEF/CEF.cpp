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

// --- Настройка заголовка ---
constexpr int HEADER_HEIGHT = 50; // доступно во всех функциях

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна

// Два браузера: основной и header (OSR)
CefRefPtr<CefBrowser> g_main_browser;
CefRefPtr<CefBrowser> g_header_browser;

// Два handler'а — по одному на браузер, чтобы в OnAfterCreated можно понять, для какого браузера коллбек
CefRefPtr<class SimpleHandler> g_main_handler;
CefRefPtr<class SimpleHandler> g_header_handler;

HWND g_hwnd = nullptr;

// draggable regions только для header (OSR)
std::vector<CefDraggableRegion> g_header_draggable_regions;
std::mutex g_header_draggable_mutex;

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
    public CefRenderHandler // <--- ДОБАВИТЬ ЭТО
{
public:
    SimpleHandler() = default;

    // --- ДОБАВИТЬ ЭТИ ПЕРЕМЕННЫЕ ---
    std::mutex render_mutex;
    std::vector<uint8_t> render_buffer;
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
    CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        // Важно: возвращать обработчик рендеринга только для header-браузера,
        // у которого включен windowless_rendering_enabled.
        if (this == g_header_handler.get()) {
            return this;
        }
        return nullptr; // Для основного браузера OSR не нужен
    }

    // Вызывается для получения размеров View. Здесь мы задаём размер нашего заголовка.
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
        RECT client_rect;
        GetClientRect(g_hwnd, &client_rect);
        rect.Set(0, 0, client_rect.right - client_rect.left, HEADER_HEIGHT);
    }

    // Главный метод: CEF передаёт нам отрендеренный кадр.
    void OnPaint(CefRefPtr<CefBrowser> browser,
        PaintElementType type,
        const RectList& dirtyRects,
        const void* buffer,
        int width,
        int height) override
    {
        // Копируем пиксели в наш внутренний буфер под мьютексом
        {
            std::lock_guard<std::mutex> lock(render_mutex);
            render_width = width;
            render_height = height;
            int buffer_size = width * height * 4; // BGRA формат
            render_buffer.resize(buffer_size);
            memcpy(render_buffer.data(), buffer, buffer_size);
        }

        // Говорим окну, что его часть (заголовок) нужно перерисовать.
        // Это вызовет сообщение WM_PAINT.
        RECT header_rect = { 0, 0, width, HEADER_HEIGHT };
        InvalidateRect(g_hwnd, &header_rect, FALSE);
    }

    // CefDragHandler: получаем drag-регионы из рендерера
    void OnDraggableRegionsChanged(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const std::vector<CefDraggableRegion>& regions) override {
        // Обновляем только для header-handler
        if (this == g_header_handler.get()) {
            std::lock_guard<std::mutex> lock(g_header_draggable_mutex);
            g_header_draggable_regions = regions;
        }
        OutputDebugStringA("OnDraggableRegionsChanged called\n");
    }

    // CefLifeSpanHandler
// In SimpleHandler class
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        if (this == g_main_handler.get()) {
            g_main_browser = browser;

            // --- ДОБАВЬТЕ ЭТОТ КОД ---
            // Принудительно устанавливаем начальный размер и позицию для
            // основного (оконного) браузера, чтобы он не перекрывал OSR-заголовок.
            if (g_hwnd) {
                RECT rc;
                GetClientRect(g_hwnd, &rc);
                HWND browser_hwnd = browser->GetHost()->GetWindowHandle();
                if (browser_hwnd) {
                    int mainHeight = rc.bottom - rc.top - HEADER_HEIGHT;
                    if (mainHeight < 0) mainHeight = 0;
                    MoveWindow(browser_hwnd, 0, HEADER_HEIGHT, rc.right - rc.left, mainHeight, TRUE);
                }
            }
            // -----------------------

        }
        else if (this == g_header_handler.get()) {
            g_header_browser = browser;
            // Для OSR-браузера это не нужно, так как у него нет своего HWND.
            // Мы просто сообщим ему об изменении размера, когда это будет необходимо.
            browser->GetHost()->WasResized();
        }
    }

    bool DoClose(CefRefPtr<CefBrowser> browser) override {
        // не форсируем закрытие здесь
        return false;
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        // Сбрасываем соответствующую глобальную ссылку
        if (this == g_main_handler.get()) {
            g_main_browser = nullptr;
        }
        else if (this == g_header_handler.get()) {
            g_header_browser = nullptr;
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
    g_header_handler = init_header_handler();

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

    int width = 800;
    int height = 600;

    HWND hWnd = CreateWindowW(
        szWindowClass,
        szTitle,
        WS_POPUP | WS_VISIBLE | WS_THICKFRAME,
        100, 100, width, height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd)
        return FALSE;

    g_hwnd = hWnd;

    RECT rc;
    GetClientRect(hWnd, &rc);

    // --- OSR заголовок сверху ---
    CefWindowInfo header_info;
    // Для OSR НЕ создаём HWND; SetAsWindowless используется:
    header_info.SetAsWindowless(g_hwnd); // <-- исправлено: 1 аргумент
    // задаём начальный размер через Send/WasResized позже (CEF возьмёт размер в момент создания)
    CefBrowserSettings header_settings;
    header_settings.windowless_frame_rate = 60; // опция, если нужно

    CefBrowserHost::CreateBrowser(header_info, g_header_handler,
        L"http://localhost:5173/header",
        header_settings, nullptr, nullptr);

    // --- Основное окно CEF (без OSR) ---
    CefWindowInfo main_info;
    main_info.SetAsChild(hWnd, CefRect(0, HEADER_HEIGHT, rc.right - rc.left, rc.bottom - rc.top - HEADER_HEIGHT));
    // windowless_rendering_enabled по умолчанию false
    CefBrowserSettings main_settings;
    CefBrowserHost::CreateBrowser(main_info, g_main_handler,
        L"http://localhost:5173/",
        main_settings, nullptr, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

// helper: возвращает host по координате y (client coords)
inline CefRefPtr<CefBrowserHost> HostForY(int y) {
    if (y < HEADER_HEIGHT) {
        if (g_header_browser) return g_header_browser->GetHost();
        return nullptr;
    }
    else {
        if (g_main_browser) return g_main_browser->GetHost();
        return nullptr;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // --- БЛОК ОТРИСОВКИ OSR ЗАГОЛОВКА ---
        if (g_header_handler) {
            std::lock_guard<std::mutex> lock(g_header_handler->render_mutex);

            if (!g_header_handler->render_buffer.empty()) {
                BITMAPINFO bmi = {};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = g_header_handler->render_width;
                // Отрицательная высота, чтобы изображение не было перевёрнутым (top-down DIB)
                bmi.bmiHeader.biHeight = -g_header_handler->render_height;
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                // Рисуем буфер на DC окна
                SetDIBitsToDevice(hdc,
                    0, 0, // Координаты X, Y
                    g_header_handler->render_width, g_header_handler->render_height,
                    0, 0,
                    0, g_header_handler->render_height,
                    g_header_handler->render_buffer.data(),
                    &bmi,
                    DIB_RGB_COLORS);
            }
        }
        // ------------------------------------

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        // header — OSR: не имеет HWND (в нашем случае SetAsWindowless), поэтому просто уведомляем WasResized
        if (g_header_browser) {
            CefRefPtr<CefBrowserHost> h = g_header_browser->GetHost();
            if (h) h->WasResized();
        }

        // main — windowed child: имеет HWND; перемещаем и уведомляем
        if (g_main_browser) {
            CefRefPtr<CefBrowserHost> h = g_main_browser->GetHost();
            if (h) {
                HWND bh = (HWND)h->GetWindowHandle();
                if (bh) {
                    int mainHeight = height - HEADER_HEIGHT;
                    if (mainHeight < 0) mainHeight = 0; // безопасный min
                    MoveWindow(bh, 0, HEADER_HEIGHT, width, mainHeight, TRUE);
                }
                h->WasResized();
            }
        }
        return 0;
    }


    case WM_ERASEBKGND:
        return 1;  // Не стираем фон, т.к. рендерим сами

    case WM_CLOSE:
        // Закрываем оба браузера корректно
        if (g_main_browser) {
            g_main_browser->GetHost()->CloseBrowser(false);
            // не делаем DestroyWindow пока браузер не закроется (OnBeforeClose)
            return 0;
        }
        // если основного браузера нет — пробуем закрыть header
        if (g_header_browser) {
            g_header_browser->GetHost()->CloseBrowser(false);
            return 0;
        }
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
        event.y = y < HEADER_HEIGHT ? y : (y - HEADER_HEIGHT); // для header координаты внутри его view, для main — relative to its child HWND
        event.modifiers = GetCefStateModifiers(wParam);

        if (message == WM_MOUSEWHEEL) {
            POINT pt = { x, y };
            ScreenToClient(hWnd, &pt);
            event.x = pt.x;
            event.y = (pt.y < HEADER_HEIGHT) ? pt.y : (pt.y - HEADER_HEIGHT);
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

            // если в header — проверяем draggable regions, чтобы вернуть HTCAPTION
            if (pt.y >= 0 && pt.y < HEADER_HEIGHT) {
                bool is_draggable = false;
                {
                    std::lock_guard<std::mutex> lock(g_header_draggable_mutex);
                    for (const auto& region : g_header_draggable_regions) {
                        const CefRect& b = region.bounds;
                        if (region.draggable) {
                            // region.bounds в координатах header view
                            if (pt.x >= b.x && pt.x < b.x + b.width &&
                                pt.y >= b.y && pt.y < b.y + b.height) {
                                is_draggable = true;
                                break;
                            }
                        }
                    }
                }
                if (is_draggable) return HTCAPTION;
            }

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
