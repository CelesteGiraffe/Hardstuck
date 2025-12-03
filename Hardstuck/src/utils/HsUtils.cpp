#include "pch.h"
#include "utils/HsUtils.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>

std::string ExtractDatePortion(const std::string& timestamp)
{
    if (timestamp.empty())
        return timestamp;

    const size_t posT = timestamp.find('T');
    if (posT != std::string::npos)
        return timestamp.substr(0, posT);

    const size_t posSpace = timestamp.find(' ');
    if (posSpace != std::string::npos)
        return timestamp.substr(0, posSpace);

    if (timestamp.size() > 10)
        return timestamp.substr(0, 10);

    return timestamp;
}

std::string FormatTimestamp(const std::chrono::system_clock::time_point& timePoint)
{
    std::time_t now = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tmUtc;
#ifdef _WIN32
    gmtime_s(&tmUtc, &now);
#else
    gmtime_r(&now, &tmUtc);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tmUtc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string FormatTimestampUk(const std::chrono::system_clock::time_point& timePoint)
{
    std::time_t now = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tmUtc;
#ifdef _WIN32
    gmtime_s(&tmUtc, &now);
#else
    gmtime_r(&now, &tmUtc);
#endif

    std::ostringstream oss;
    // UK-style: DD/MM/YYYY HH:MM:SS (24h)
    oss << std::put_time(&tmUtc, "%d/%m/%Y %H:%M:%S");
    return oss.str();
}

std::string FormatTimestampStringUk(const std::string& timestamp)
{
    if (timestamp.empty())
    {
        return timestamp;
    }

    std::tm tmUtc{};
    std::istringstream iss(timestamp);
    iss >> std::get_time(&tmUtc, "%Y-%m-%dT%H:%M:%SZ");
    if (iss.fail())
    {
        return timestamp;
    }

    std::ostringstream oss;
    oss << std::put_time(&tmUtc, "%d/%m/%Y %H:%M:%S");
    return oss.str();
}

std::string JsonEscape(const std::string& value)
{
    std::ostringstream oss;
    oss << '"';
    for (char c : value)
    {
        switch (c)
        {
        case '\\': oss << "\\\\"; break;
        case '"':  oss << "\\\""; break;
        case '\n': oss << "\\n";  break;
        case '\r': oss << "\\r";  break;
        case '\t': oss << "\\t";  break;
        default:   oss << c;      break;
        }
    }
    oss << '"';
    return oss.str();
}
