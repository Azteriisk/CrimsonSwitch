#include "Logger.hpp"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <cstdarg>
#include <chrono>
#include <iomanip>
#include <mutex>

namespace CrimsonSwitch {

std::string Logger::s_logPath = "CrimsonSwitch.log";
std::string Logger::s_secondaryPath = "C:\\Users\\thera\\code\\CrimsonSwitch\\CrimsonSwitch.log";
bool Logger::s_isInitialized = false;
static std::mutex s_logMutex;

void Logger::Init(const std::string& filename) {
    std::lock_guard<std::mutex> lock(s_logMutex);
    s_logPath = filename;

    std::ofstream f1(s_logPath, std::ios::out | std::ios::trunc);
    if (f1.is_open()) {
        f1 << "=== Crimson Desert Early Trio Switch Mod Log Initialized ===\n";
        f1.flush();
    }

    std::ofstream f2(s_secondaryPath, std::ios::out | std::ios::trunc);
    if (f2.is_open()) {
        f2 << "=== Crimson Desert Early Trio Switch Mod Log Initialized ===\n";
        f2.flush();
    }

    s_isInitialized = true;
}

void Logger::Log(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(s_logMutex);

    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm timeInfo{};
    localtime_s(&timeInfo, &in_time_t);

    std::ostringstream oss;
    oss << "[" << std::put_time(&timeInfo, "%H:%M:%S") << "."
        << std::setfill('0') << std::setw(3) << ms.count() << "] "
        << "[TID:" << GetCurrentThreadId() << "] "
        << buffer;

    std::string formatted = oss.str();

    OutputDebugStringA(formatted.c_str());

    std::ofstream f1(s_logPath, std::ios::out | std::ios::app);
    if (f1.is_open()) {
        f1 << formatted;
        f1.flush();
    }

    std::ofstream f2(s_secondaryPath, std::ios::out | std::ios::app);
    if (f2.is_open()) {
        f2 << formatted;
        f2.flush();
    }
}

void Logger::Shutdown() {
    Log("[Core] Logger shutting down.\n");
    s_isInitialized = false;
}

} // namespace CrimsonSwitch
