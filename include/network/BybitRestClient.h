#pragma once
#include <vector>
#include <string>

class BybitRestClient {
public:
    static std::vector<std::string> fetch_all_usdt_symbols();

private:
    static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, std::string* output);
};
