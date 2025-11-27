#pragma once

#include <string>
#include <vector>

struct HttpHeader {
    std::string name;
    std::string value;

    HttpHeader() = default;
    HttpHeader(std::string n, std::string v) : name(std::move(n)), value(std::move(v)) {}
};

class ApiClient {
public:
    ApiClient(std::string baseUrl);
    void SetBaseUrl(std::string newBaseUrl);
    std::string NormalizeBaseUrl(const std::string& url) const;
    std::string BuildUrl(const std::string& endpoint) const;
    bool PostJson(const std::string& endpoint,
                  const std::string& body,
                  const std::vector<HttpHeader>& headers,
                  std::string& response,
                  std::string& error) const;
    bool GetJson(const std::string& endpoint,
                 const std::vector<HttpHeader>& headers,
                 std::string& response,
                 std::string& error) const;

private:
    bool SendRequest(const std::string& method,
                     const std::string& endpoint,
                     const std::vector<HttpHeader>& headers,
                     const std::string* body,
                     std::string& response,
                     std::string& error) const;
    std::string baseUrl;
};
