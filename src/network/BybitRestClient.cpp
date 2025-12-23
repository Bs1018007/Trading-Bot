#include "network/BybitRestClient.h"
#include <curl/curl.h>
#include <simdjson.h>
#include <iostream>

size_t BybitRestClient::curl_write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::vector<std::string> BybitRestClient::fetch_all_usdt_symbols() {
    std::vector<std::string> symbols;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL\n";
        return symbols;
    }

    std::string response;
    std::string api_url = "https://api.bybit.com/v5/market/instruments-info?category=linear&limit=1000";
    
    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: BybitBot/1.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::cout << "🔄 Making API request to Bybit...\n";
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "❌ CURL error: " << curl_easy_strerror(res) << "\n";
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return symbols;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    std::cout << "📊 HTTP Response Code: " << http_code << "\n";

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (response.empty()) {
        std::cerr << "❌ Empty response from Bybit API\n";
        return symbols;
    }

    std::cout << "📝 Response size: " << response.length() << " bytes\n";
    std::cout << "\n📋 RAW JSON RESPONSE (first 2000 chars):\n";
    std::cout << response.substr(0, 2000) << "\n\n";

    try {
        simdjson::ondemand::parser parser;
        simdjson::padded_string padded(response);
        simdjson::ondemand::document doc = parser.iterate(padded);

        auto status = doc["retCode"];
        if (!status.error()) {
            uint64_t code = status.get_uint64().value();
            if (code != 0) {
                std::cerr << "❌ Bybit API error code: " << code << "\n";
                auto msg = doc["retMsg"];
                if (!msg.error()) {
                    std::cerr << "   Message: " << msg.get_string().value() << "\n";
                }
                return symbols;
            }
        }

        auto result = doc["result"];
        if (result.error()) {
            std::cerr << "❌ No 'result' field in response\n";
            return symbols;
        }

        auto list = result["list"];
        if (list.error()) {
            std::cerr << "❌ No 'list' field in result\n";
            return symbols;
        }

        int count = 0;
        for (auto item : list) {
            auto symbol_result = item["symbol"];
            if (!symbol_result.error()) {
                std::string_view sym = symbol_result.get_string().value();
                if (sym.find("USDT") != std::string::npos && 
                    sym.find("10") == std::string::npos) {
                    symbols.push_back(std::string(sym));
                    count++;
                    if (count % 10 == 0) {
                        std::cout << "  ✓ Loaded " << count << " symbols...\n";
                    }
                }
            }
        }
        
        std::cout << "\n✓ Successfully fetched " << symbols.size() << " USDT trading pairs from Bybit\n\n";
    } catch (const simdjson::simdjson_error& e) {
        std::cerr << "❌ JSON parse error: " << e.what() << "\n";
        std::cerr << "   Response (first 500 chars): " << response.substr(0, 500) << "\n";
    }

    return symbols;
}