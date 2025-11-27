#include "pch.h"
#include "diagnostics/DiagnosticLogger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <cstdlib>

static std::mutex g_logMutex;
static std::string g_logPath;

void DiagnosticLogger::Init()
{
    try {
#ifdef _WIN32
        char* appdata_env = nullptr;
        size_t env_len = 0;
        std::string appdata_str;
        if (_dupenv_s(&appdata_env, &env_len, "APPDATA") == 0 && appdata_env && appdata_env[0] != '\0') {
            appdata_str = appdata_env;
        }
        if (appdata_env) {
            free(appdata_env);
        }
        std::filesystem::path base;
        if (!appdata_str.empty()) {
            base = appdata_str;
        } else {
            base = std::filesystem::temp_directory_path();
        }
#else
        const char* appdata = std::getenv("APPDATA");
        std::filesystem::path base;
        if (appdata && appdata[0] != '\0') {
            base = appdata;
        } else {
            base = std::filesystem::temp_directory_path();
        }
#endif

        std::filesystem::path dir = base / "bakkesmod" / "hardstuck_logs";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d-%H%M%S");
        std::string filename = std::string("hardstuck_") + oss.str() + ".log";

        g_logPath = (dir / filename).string();

        // Write header
        std::ofstream ofs(g_logPath, std::ios::out | std::ios::app);
        if (ofs) {
            ofs << "--- Hardstuck Diagnostic Log " << oss.str() << " ---\n";
            ofs.close();
        }
    } catch (...) {
        // Best-effort; do not throw
    }
}

void DiagnosticLogger::Log(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    try {
        if (g_logPath.empty()) {
            Init();
        }
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " - " << msg << "\n";

        std::ofstream ofs(g_logPath, std::ios::out | std::ios::app);
        if (ofs) {
            ofs << oss.str();
        }
    } catch (...) {
        // swallow errors; logging must not crash plugin
    }
}
