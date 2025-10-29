#pragma once
#include <iostream>
#include "include/cef_render_process_handler.h"
#include "include/cef_app.h"
#include "include/cef_v8.h"
#include "Api.h"
#include "include/cef_task.h"

// Обработчик JS → Python
class JsBridge : public CefV8Handler {
public:
    bool Execute(const CefString& name,
        CefRefPtr<CefV8Value> object,
        const CefV8ValueList& arguments,
        CefRefPtr<CefV8Value>& retval,
        CefString& exception) override {
        if (name == "sendToCpp") {
            if (arguments.size() == 2 && arguments[0]->IsString() && arguments[1]->IsFunction()) {
                std::string input = arguments[0]->GetStringValue();
                CefRefPtr<CefV8Value> callback = arguments[1];
                CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();

                // Запускаем асинхронную задачу для вызова Python
                CefPostTask(TID_RENDERER, new PythonTask(input, context, callback));
                return true;
            }
            exception = "Invalid arguments: expected a string and a callback function";
            return true;
        }
        return false;
    }

private:
    // Класс задачи для асинхронного вызова Python
    class PythonTask : public CefTask {
    public:
        PythonTask(const std::string& input,
            CefRefPtr<CefV8Context> context,
            CefRefPtr<CefV8Value> callback)
            : input_(input), context_(context), callback_(callback) {
        }

        void Execute() override {
            // Вызываем Python синхронно в рамках этой задачи
            std::string result = CallPython(input_);

            // Возвращаемся в контекст V8 для вызова callback
            if (context_->Enter()) {
                CefV8ValueList args;
                bool success = result.find("\"error\"") == std::string::npos;

                if (success) {
                    // Успех: передаём результат как первый аргумент (null, result)
                    args.push_back(CefV8Value::CreateNull());
                    args.push_back(CefV8Value::CreateString(result));
                }
                else {
                    // Ошибка: передаём ошибку как первый аргумент (error, null)
                    args.push_back(CefV8Value::CreateString(result));
                    args.push_back(CefV8Value::CreateNull());
                }

                // Вызываем callback
                callback_->ExecuteFunctionWithContext(context_, nullptr, args);
                context_->Exit();
            }
        }

    private:
        std::string input_;
        CefRefPtr<CefV8Context> context_;
        CefRefPtr<CefV8Value> callback_;
        IMPLEMENT_REFCOUNTING(PythonTask);
    };

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