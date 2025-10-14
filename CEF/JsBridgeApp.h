#pragma once
#include <iostream>
#include "include/cef_render_process_handler.h"
#include "include/cef_app.h"
#include "include/cef_v8.h"
#include "Api.h"

// Обработчик JS → Python
class JsBridge : public CefV8Handler {
public:
    bool Execute(const CefString& name,
        CefRefPtr<CefV8Value> object,
        const CefV8ValueList& arguments,
        CefRefPtr<CefV8Value>& retval,
        CefString& exception) override {
        if (name == "sendToCpp") {
            if (arguments.size() == 1 && arguments[0]->IsString()) {
                std::string input = arguments[0]->GetStringValue();
                std::string pyResult = CallPython(input);
                retval = CefV8Value::CreateString(pyResult);
                return true;
            }
        }
        return false;
    }

    IMPLEMENT_REFCOUNTING(JsBridge);
};

// Основное приложение CEF, создающее JS-функцию
class JsBridgeApp : public CefApp, public CefRenderProcessHandler {
public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

    void OnContextCreated(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override {

        static bool python_initialized = false;
        if (!python_initialized) {
            python_initialized = InitPythonApi();
            if (!python_initialized)
                std::cerr << "[Renderer] Failed to initialize Python API" << std::endl;
            else
                std::cout << "[Renderer] Python API initialized successfully" << std::endl;
        }

        CefRefPtr<CefV8Value> global = context->GetGlobal();
        CefRefPtr<JsBridge> handler = new JsBridge();
        CefRefPtr<CefV8Value> func = CefV8Value::CreateFunction("sendToCpp", handler);
        global->SetValue("sendToCpp", func, V8_PROPERTY_ATTRIBUTE_NONE);
    }


    IMPLEMENT_REFCOUNTING(JsBridgeApp);
};