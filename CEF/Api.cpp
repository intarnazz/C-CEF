#include "Api.h"
#include <Python.h>
#include <iostream>
#include <mutex>

static PyObject* pModule = nullptr;
static PyObject* pFunc = nullptr;
static std::mutex python_mutex;

bool InitPythonApi() {
    std::lock_guard<std::mutex> lock(python_mutex);
    if (Py_IsInitialized()) {
        return true;  // Already initialized (idempotent)
    }
    PyStatus status;
    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    PyWideStringList_Append(&config.module_search_paths, L"./python313.zip");
    PyWideStringList_Append(&config.module_search_paths, L"./DLLs");
    Py_Initialize();
    PyRun_SimpleString("import sys; sys.path.append('.')");

    PyObject* pName = PyUnicode_FromString("api");
    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (!pModule) {
        PyErr_Print();
        std::cerr << "[Python] Не удалось загрузить Api.py\n";
        return false;
    }

    pFunc = PyObject_GetAttrString(pModule, "handle_event");
    if (!pFunc || !PyCallable_Check(pFunc)) {
        std::cerr << "[Python] Функция handle_event не найдена\n";
        return false;
    }

    std::cout << "[Python] Api успешно инициализирован\n";
    std::atexit(ShutdownPythonApi);  // Register shutdown
    return true;
}

std::string CallPython(const std::string& json) {
    std::lock_guard<std::mutex> lock(python_mutex);
    if (!pFunc) return "{\"error\":\"Python API не инициализирован\"}";

    PyObject* pArgs = PyTuple_Pack(1, PyUnicode_FromString(json.c_str()));
    PyObject* pValue = PyObject_CallObject(pFunc, pArgs);
    Py_DECREF(pArgs);

    if (pValue) {
        std::string result = PyUnicode_AsUTF8(pValue);
        Py_DECREF(pValue);
        return result;
    }
    else {
        PyErr_Print();
        return "{\"error\":\"Ошибка в Python\"}";
    }
}

void ShutdownPythonApi() {
    std::lock_guard<std::mutex> lock(python_mutex);
    Py_XDECREF(pFunc);
    Py_XDECREF(pModule);
    if (Py_IsInitialized()) Py_Finalize();
}