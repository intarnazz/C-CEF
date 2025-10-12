// CEF.cpp : Определяет точку входа для приложения.
//

// вверху файла — WIN32 defines уже у тебя есть:
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Windowsx.h> // если нужен - но он и вызывает многие макросы

// Сохраняем и временно убираем конфликтные макросы:
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

// Теперь безопасно подключаем CEF:
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_browser.h"
#include "include/base/cef_scoped_refptr.h"
#include "include/internal/cef_types_wrappers.h"
#include "include/cef_base.h"
#include "include/cef_dom.h" // если нужен

// Восстанавливаем макросы в прежнее состояние:
#pragma pop_macro("GetWindowText")
#pragma pop_macro("GetObject")
#pragma pop_macro("SetParent")
#pragma pop_macro("GetClassName")
#pragma pop_macro("GetLastChild")
#pragma pop_macro("GetParent")
#pragma pop_macro("GetFirstChild")
#pragma pop_macro("GetNextSibling")
#include "resource.h"



#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна
CefRefPtr<CefBrowser> g_browser;

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// Добавьте этот класс в отдельный .h/.cpp файл, но для примера пусть будет здесь
class SimpleHandler : public CefClient, public CefLifeSpanHandler {
public:
    // ИЗМЕНЕНИЕ: Заменить OVERRIDE на override
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return CefRefPtr<CefLifeSpanHandler>(this);
    }

    // ИЗМЕНЕНИЕ: Заменить OVERRIDE на override
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        g_browser = browser;
    }

    // ИЗМЕНЕНИЕ: Заменить OVERRIDE на override
    bool DoClose(CefRefPtr<CefBrowser> browser) override {
        return false;
    }

    // ИЗМЕНЕНИЕ: Заменить OVERRIDE на override
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        g_browser = nullptr;
    }

    // Макрос IMPLEMENT_REFCOUNTING из "include/base/cef_base.h"
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

    // Создайте заглушку CefApp (если не нужны кастомные обработчики)
    CefRefPtr<CefApp> app;

    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    CefInitialize(main_args, settings, app, nullptr);

    // TODO: Разместите код здесь.

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CEF, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Выполнить инициализацию приложения:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CEF));

    MSG msg;

    // Цикл основного сообщения:
    while (GetMessage(&msg, nullptr, 0, 0))
    {

        // Обработка сообщений CEF
        CefDoMessageLoopWork();

        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

    }

    // Завершение работы CEF при выходе из цикла
    CefShutdown();

    return (int)msg.wParam;
}



//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  ЦЕЛЬ: Регистрирует класс окна.
//
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

//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   ЦЕЛЬ: Сохраняет маркер экземпляра и создает главное окно
//
//   КОММЕНТАРИИ:
//
//        В этой функции маркер экземпляра сохраняется в глобальной переменной, а также
//        создается и выводится главное окно программы.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    // Прямо задаём размер и позицию
    int width = 800;
    int height = 600;

    HWND hWnd = CreateWindowW(
        szWindowClass,
        szTitle,
        WS_POPUP | WS_THICKFRAME | WS_VISIBLE, // <---- вот здесь добавили WS_THICKFRAME
        100, 100, 800, 600,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd)
        return FALSE;

    // --- Добавление CEF ---
// Настройки создания браузера
    CefBrowserSettings browser_settings;
    CefWindowInfo window_info;

    // Определяем размеры окна WinAPI
    RECT browser_rect = { 0, 0, 800, 600 };

    // ПРЕОБРАЗОВАНИЕ: Создаем CefRect из WinAPI RECT
    CefRect cef_rect(browser_rect.left, browser_rect.top,
        browser_rect.right - browser_rect.left,
        browser_rect.bottom - browser_rect.top);

    // Прикрепляем браузер к нашему HWND в качестве дочернего окна
    window_info.SetAsChild(hWnd, cef_rect); // <--- Используем cef_rect

    // Создаём браузер с начальным URL и нашим обработчиком
    CefBrowserHost::CreateBrowser(window_info, g_handler,
        L"https://www.google.com", // Используем L"" для CefString
        browser_settings, nullptr, nullptr);
    // -----------------------

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    return TRUE;
}


//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ЦЕЛЬ: Обрабатывает сообщения в главном окне.
//
//  WM_COMMAND  - обработать меню приложения
//  WM_PAINT    - Отрисовка главного окна
//  WM_DESTROY  - отправить сообщение о выходе и вернуться
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (g_browser.get()) {
            RECT rect;
            GetClientRect(hWnd, &rect);

            // Получаем HWND окна CEF
            HWND cef_hwnd = g_browser->GetHost()->GetWindowHandle();

            // Используем WinAPI для изменения размера дочернего окна
            SetWindowPos(cef_hwnd,
                NULL,
                0, 0,
                rect.right - rect.left,
                rect.bottom - rect.top,
                SWP_NOZORDER);
        }
        break;

    case WM_CLOSE:
        // Сначала просим CEF закрыть браузер
        if (g_browser.get()) {
            g_browser->GetHost()->CloseBrowser(false);
            // DefWindowProc не вызываем, т.к. закрытие окна WinAPI произойдет
            // в OnBeforeClose после закрытия браузера.
            return 0;
        }
        break;
    case WM_NCHITTEST:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);

        RECT rc;
        GetClientRect(hWnd, &rc);

        int borderWidth = 10; // <-- вот здесь регулируешь толщину рамки (по вкусу)

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

        // Если не попали в рамку, позволяем двигать окно
        return HTCAPTION;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Разобрать выбор в меню:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: Добавьте сюда любой код прорисовки, использующий HDC...
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений для окна "О программе".
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