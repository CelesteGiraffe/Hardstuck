#include "pch.h"
#include "backend/ApiClient.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace
{
#ifdef _WIN32
    struct ParsedUrl
    {
        bool secure;
        std::wstring host;
        INTERNET_PORT port;
        std::wstring path;
    };

    std::wstring ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return std::wstring();
        }

        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (sizeNeeded <= 0)
        {
            return std::wstring();
        }

        std::wstring result;
        result.resize(sizeNeeded);
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), sizeNeeded);
        return result;
    }

    bool ParseUrl(const std::string& url, ParsedUrl& parsed, std::string& error)
    {
        std::string working = url;
        parsed.secure = false;

        const std::string https = "https://";
        const std::string http = "http://";

        if (working.rfind(https, 0) == 0)
        {
            parsed.secure = true;
            working.erase(0, https.size());
        }
        else if (working.rfind(http, 0) == 0)
        {
            working.erase(0, http.size());
        }
        else
        {
            error = "URL must start with http:// or https://";
            return false;
        }

        std::string::size_type slashPos = working.find('/');
        std::string hostPort = slashPos == std::string::npos ? working : working.substr(0, slashPos);
        std::string path = slashPos == std::string::npos ? "/" : working.substr(slashPos);

        if (hostPort.empty())
        {
            error = "URL missing host";
            return false;
        }

        INTERNET_PORT port = parsed.secure ? 443 : 80;
        std::string::size_type colonPos = hostPort.find(':');
        if (colonPos != std::string::npos)
        {
            std::string portString = hostPort.substr(colonPos + 1);
            hostPort = hostPort.substr(0, colonPos);
            try
            {
                port = static_cast<INTERNET_PORT>(std::stoi(portString));
            }
            catch (const std::exception&)
            {
                error = "Invalid port in URL";
                return false;
            }
        }

        parsed.host = ToWide(hostPort);
        parsed.port = port;
        parsed.path = ToWide(path);
        return true;
    }
#endif

} // namespace

ApiClient::ApiClient(std::string baseUrl)
{
    SetBaseUrl(std::move(baseUrl));
}

void ApiClient::SetBaseUrl(std::string newBaseUrl)
{
    baseUrl = NormalizeBaseUrl(std::move(newBaseUrl));
}

std::string ApiClient::NormalizeBaseUrl(const std::string& url) const
{
    if (url.empty())
    {
        return std::string();
    }

    std::string trimmed = url;
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
                      return !std::isspace(ch);
                  }));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) {
                      return !std::isspace(ch);
                  }).base(),
                  trimmed.end());

    while (!trimmed.empty() && trimmed.back() == '/')
    {
        trimmed.pop_back();
    }

    return trimmed;
}

std::string ApiClient::BuildUrl(const std::string& endpoint) const
{
    if (baseUrl.empty())
    {
        return endpoint;
    }

    if (endpoint.empty())
    {
        return baseUrl;
    }

    if (endpoint.front() == '/')
    {
        return baseUrl + endpoint;
    }

    return baseUrl + "/" + endpoint;
}

bool ApiClient::SendRequest(const std::string& method,
                            const std::string& endpoint,
                            const std::vector<HttpHeader>& headers,
                            const std::string* body,
                            std::string& response,
                            std::string& error) const
{
#ifdef _WIN32
    if (baseUrl.empty())
    {
        error = "API base URL is empty";
        return false;
    }

    const std::string url = BuildUrl(endpoint);

    ParsedUrl parsed;
    if (!ParseUrl(url, parsed, error))
    {
        return false;
    }

    HINTERNET session = WinHttpOpen(L"HardstuckPlugin/1.0",
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (!session)
    {
        error = "WinHttpOpen failed: " + std::to_string(GetLastError());
        return false;
    }

    HINTERNET connection = WinHttpConnect(session, parsed.host.c_str(), parsed.port, 0);
    if (!connection)
    {
        error = "WinHttpConnect failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD flags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wideMethod = ToWide(method);
    if (wideMethod.empty())
    {
        error = "HTTP method is empty";
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(connection,
                                           wideMethod.c_str(),
                                           parsed.path.c_str(),
                                           nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           flags);
    if (!request)
    {
        error = "WinHttpOpenRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    for (const auto& header : headers)
    {
        if (header.name.empty())
        {
            continue;
        }

        std::string joined = header.name + ": " + header.value + "\r\n";
        std::wstring wideHeader = ToWide(joined);
        if (!wideHeader.empty())
        {
            WinHttpAddRequestHeaders(request, wideHeader.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD);
        }
    }

    const bool hasBody = body != nullptr && !body->empty();
    BOOL result = WinHttpSendRequest(request,
                                     WINHTTP_NO_ADDITIONAL_HEADERS,
                                     0,
                                     hasBody ? (LPVOID)body->data() : WINHTTP_NO_REQUEST_DATA,
                                     hasBody ? static_cast<DWORD>(body->size()) : 0,
                                     hasBody ? static_cast<DWORD>(body->size()) : 0,
                                     0);
    if (!result)
    {
        error = "WinHttpSendRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    result = WinHttpReceiveResponse(request, nullptr);
    if (!result)
    {
        error = "WinHttpReceiveResponse failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &statusCode,
                             &statusSize,
                             WINHTTP_NO_HEADER_INDEX))
    {
        error = "Unable to query HTTP status code: " + std::to_string(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    std::ostringstream responseStream;
    DWORD availableBytes = 0;
    do
    {
        if (!WinHttpQueryDataAvailable(request, &availableBytes))
        {
            error = "WinHttpQueryDataAvailable failed: " + std::to_string(GetLastError());
            break;
        }

        if (!availableBytes)
        {
            break;
        }

        std::string buffer;
        buffer.resize(availableBytes);
        DWORD downloaded = 0;
        if (!WinHttpReadData(request, buffer.data(), availableBytes, &downloaded))
        {
            error = "WinHttpReadData failed: " + std::to_string(GetLastError());
            break;
        }

        responseStream.write(buffer.data(), downloaded);
    } while (availableBytes > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    const bool success = statusCode >= 200 && statusCode < 300;
    response = responseStream.str();
    if (success)
    {
        error.clear();
        return true;
    }

    std::ostringstream oss;
    oss << "HTTP " << statusCode;
    if (!response.empty())
    {
        oss << ": " << response;
    }
    error = oss.str();
    return false;
#else
    (void)method;
    (void)endpoint;
    (void)headers;
    (void)body;
    error = "HTTP client is only available on Windows";
    return false;
#endif
}

bool ApiClient::PostJson(const std::string& endpoint,
                         const std::string& body,
                         const std::vector<HttpHeader>& headers,
                         std::string& response,
                         std::string& error) const
{
    std::vector<HttpHeader> requestHeaders = headers;
    requestHeaders.emplace_back("Content-Type", "application/json");
    return SendRequest("POST", endpoint, requestHeaders, &body, response, error);
}

bool ApiClient::GetJson(const std::string& endpoint,
                        const std::vector<HttpHeader>& headers,
                        std::string& response,
                        std::string& error) const
{
    return SendRequest("GET", endpoint, headers, nullptr, response, error);
}
