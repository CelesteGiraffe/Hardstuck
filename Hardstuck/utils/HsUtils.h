#pragma once
#include <string>
#include <chrono>

std::string ExtractDatePortion(const std::string& timestamp);
std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp);
std::string JsonEscape(const std::string& value);
