#pragma once

#include <string>

class DiagnosticLogger {
public:
    // Initialize logger; called once from plugin startup.
    static void Init();

    // Thread-safe append message with timestamp.
    static void Log(const std::string& msg);
};
