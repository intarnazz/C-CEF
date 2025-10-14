#pragma once
#include <string>

bool InitPythonApi();
std::string CallPython(const std::string& json);
void ShutdownPythonApi();
