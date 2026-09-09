#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <sstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

using tcp = asio::ip::tcp;

// ============================================================
// HTTP GET - ONLY FOR CHROME CDP HTTP INTERFACE
// ============================================================

static size_t WriteCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userp)
{
    size_t total = size * nmemb;

    std::string* output =
        static_cast<std::string*>(userp);

    output->append(
        static_cast<char*>(contents),
        total);

    return total;
}

// ============================================================
// SIMPLE HTTP GET USING CURL
// ============================================================

#include <curl/curl.h>

static bool HttpGet(
    const std::string& url,
    std::string& response)
{
    CURL* curl = curl_easy_init();

    if (!curl)
        return false;

    response.clear();

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.c_str());

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        WriteCallback);

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &response);

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        5L);

    CURLcode rc =
        curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    return rc == CURLE_OK &&
           !response.empty();
}

// ============================================================
// EXTRACT JSON STRING VALUE
// ============================================================

static std::string ExtractJsonString(
    const std::string& json,
    const std::string& key)
{
    const std::string marker =
        "\"" + key + "\"";

    size_t p = json.find(marker);

    if (p == std::string::npos)
        return "";

    p += marker.size();

    // Skip whitespace
    while (p < json.size() &&
           (json[p] == ' ' ||
            json[p] == '\t' ||
            json[p] == '\r' ||
            json[p] == '\n'))
    {
        ++p;
    }

    if (p >= json.size() || json[p] != ':')
        return "";

    ++p;

    // Skip whitespace after :
    while (p < json.size() &&
           (json[p] == ' ' ||
            json[p] == '\t' ||
            json[p] == '\r' ||
            json[p] == '\n'))
    {
        ++p;
    }

    if (p >= json.size() || json[p] != '"')
        return "";

    ++p;

    std::string value;
    bool escaped = false;

    for (size_t i = p;
         i < json.size();
         ++i)
    {
        char c = json[i];

        if (escaped)
        {
            switch (c)
            {
                case 'n': value += '\n'; break;
                case 'r': value += '\r'; break;
                case 't': value += '\t'; break;
                case '"': value += '"'; break;
                case '\\': value += '\\'; break;
                case '/': value += '/'; break;
                case 'b': value += '\b'; break;
                case 'f': value += '\f'; break;
                default: value += c; break;
            }

            escaped = false;
            continue;
        }

        if (c == '\\')
        {
            escaped = true;
            continue;
        }

        if (c == '"')
            break;

        value += c;
    }

    return value;
}

// ============================================================
// CDP COMMAND
// ============================================================

static bool SendCommand(
    websocket::stream<tcp::socket>& ws,
    int id,
    const std::string& method,
    const std::string& params,
    const std::string& sessionId,
    std::string& response)
{
    std::ostringstream msg;

    msg
        << "{"
        << "\"id\":" << id << ","
        << "\"method\":\"" << method << "\"";

    if (!params.empty())
    {
        msg
            << ","
            << "\"params\":"
            << params;
    }

    if (!sessionId.empty())
    {
        msg
            << ","
            << "\"sessionId\":\""
            << sessionId
            << "\"";
    }

    msg << "}";

    ws.write(
        asio::buffer(msg.str()));

    for (;;)
    {
        beast::flat_buffer buffer;

        ws.read(buffer);

        response =
            beast::buffers_to_string(
                buffer.data());

        std::string idMarker =
            "\"id\":" +
            std::to_string(id);

        if (response.find(idMarker)
            != std::string::npos)
        {
            return true;
        }
    }
}

// ============================================================
// GET CHROME BROWSER WEBSOCKET
// ============================================================

static bool GetBrowserWebSocket(
    std::string& wsUrl)
{
    std::string version;

    std::cout
        << "Checking Chrome CDP...\n";

    const std::string endpoints[] =
    {
        "http://127.0.0.1:9222",
        "http://172.31.96.1:9223"
    };

    for (const auto& base : endpoints)
    {
        std::cout
            << "Trying : "
            << base
            << "\n";

        if (!HttpGet(
                base + "/json/version",
                version))
        {
            continue;
        }

        wsUrl =
            ExtractJsonString(
                version,
                "webSocketDebuggerUrl");

        if (!wsUrl.empty())
        {
            std::cout
                << "CDP OK : "
                << base
                << "\n";

            // Chrome reports 127.0.0.1.
            // Replace it with WSL-accessible address.
            const std::string chromePrefix =
                "ws://127.0.0.1:9222";

            if (wsUrl.rfind(
                    chromePrefix,
                    0) == 0)
            {
                wsUrl =
                    "ws://172.31.96.1:9223" +
                    wsUrl.substr(
                        chromePrefix.size());
            }

            return true;
        }
    }

    return false;
}

// ============================================================
// PARSE WS URL
// ============================================================

static bool ParseWsUrl(
    const std::string& wsUrl,
    std::string& host,
    std::string& port,
    std::string& target)
{
    if (wsUrl.rfind(
            "ws://",
            0) != 0)
    {
        return false;
    }

    std::string s =
        wsUrl.substr(5);

    size_t slash =
        s.find('/');

    if (slash == std::string::npos)
        return false;

    std::string hostPort =
        s.substr(0, slash);

    target =
        s.substr(slash);

    size_t colon =
        hostPort.rfind(':');

    if (colon == std::string::npos)
        return false;

    host =
        hostPort.substr(0, colon);

    port =
        hostPort.substr(colon + 1);

    return true;
}

// ============================================================
// FETCH SET JSON THROUGH CHROME
// ============================================================

static bool FetchJson(
    websocket::stream<tcp::socket>& ws,
    const std::string& sessionId,
    int commandId,
    const std::string& url,
    std::string& output)
{
    std::cout
        << "\nFetching:\n"
        << url
        << "\n";

    const std::string expression =
        "fetch('" +
        url +
        "',{credentials:'include'})"
        ".then(r=>r.text())"
        ".then(x=>x)";

    const std::string params =
        "{"
        "\"expression\":\"" +
        expression +
        "\","
        "\"awaitPromise\":true,"
        "\"returnByValue\":true"
        "}";

    std::string response;

    if (!SendCommand(
            ws,
            commandId,
            "Runtime.evaluate",
            params,
            sessionId,
            response))
    {
        return false;
    }

    std::cout
        << "\nFETCH RESPONSE:\n"
        << response.substr(
            0,
            std::min<size_t>(
                response.size(),
                1000))
        << "\n";

    const std::string valueKey =
        "\"value\":\"";

    const size_t valuePos =
        response.find(valueKey);

    if (valuePos == std::string::npos)
    {
        std::cerr
            << "ERROR: fetch returned no value\n";

        return false;
    }

    const size_t startValue =
        valuePos + valueKey.size();

    bool escaped = false;

    output.clear();

    for (size_t i = startValue;
         i < response.size();
         ++i)
    {
        const char c = response[i];

        if (escaped)
        {
            if (c == 'n')
                output += '\n';
            else if (c == 'r')
                output += '\r';
            else if (c == 't')
                output += '\t';
            else
                output += c;

            escaped = false;
            continue;
        }

        if (c == '\\')
        {
            escaped = true;
            continue;
        }

        if (c == '"')
            break;

        output += c;
    }

    if (output.empty())
    {
        std::cerr
            << "ERROR: fetch response body empty\n";

        return false;
    }

    return true;
}
 
// ============================================================
// MAIN
// ============================================================

int main()
{
    std::cout
        << "========================================\n"
        << "C++ SET FUNDAMENTAL JSON CAPTURE TEST\n"
        << "========================================\n";

    // ========================================================
    // 1. Chrome CDP
    // ========================================================

    std::string browserWs;

    if (!GetBrowserWebSocket(browserWs))
    {
        std::cerr
            << "ERROR: Browser WebSocket not found\n";

        return 1;
    }

    std::cout
        << "Browser WS : "
        << browserWs
        << "\n";

    // ========================================================
    // 2. Connect Browser WebSocket
    // ========================================================

    std::string host;
    std::string port;
    std::string target;

    if (!ParseWsUrl(
            browserWs,
            host,
            port,
            target))
    {
        std::cerr
            << "ERROR: Invalid browser WebSocket\n";

        return 1;
    }

    asio::io_context ioc;

    tcp::resolver resolver(ioc);

    websocket::stream<tcp::socket> ws(ioc);

    auto endpoints =
        resolver.resolve(
            host,
            port);

    asio::connect(
        ws.next_layer(),
        endpoints);

    ws.handshake(
        host + ":" + port,
        target);

    std::cout
        << "Browser WebSocket : CONNECTED\n";

    // ========================================================
    // 3. Create new Chrome target
    // ========================================================

    const std::string symbol =
        "AOT";

    const std::string financialUrl =
        "https://www.set.or.th/api/set/stock/" +
        symbol +
        "/company-highlight/financial-data?lang=th";

    const std::string highlightUrl =
        "https://www.set.or.th/api/set/stock/" +
        symbol +
        "/company-highlight/trading-stat?lang=th";

        const std::string historicalUrl =
    "https://www.set.or.th/api/set/stock/" +
    symbol +
    "/historical-trading?lang=th";

    std::string createResponse;

    std::string createParams =
        "{"
        "\"url\":\"about:blank\""
        "}";

    if (!SendCommand(
            ws,
            1,
            "Target.createTarget",
            createParams,
            "",
            createResponse))
    {
        std::cerr
            << "ERROR: Cannot create Chrome target\n";

        return 1;
    }

    std::string targetId =
        ExtractJsonString(
            createResponse,
            "targetId");

    if (targetId.empty())
    {
        std::cerr
            << "ERROR: targetId not found\n"
            << createResponse
            << "\n";

        return 1;
    }

    std::cout
        << "Chrome target created\n"
        << "Target ID : "
        << targetId
        << "\n";

    // ========================================================
    // 4. Attach
    // ========================================================

    std::string attachResponse;

    std::string attachParams =
        "{"
        "\"targetId\":\"" +
        targetId +
        "\","
        "\"flatten\":true"
        "}";

    if (!SendCommand(
            ws,
            2,
            "Target.attachToTarget",
            attachParams,
            "",
            attachResponse))
    {
        std::cerr
            << "ERROR: Cannot attach target\n";

        return 1;
    }

    std::string sessionId =
        ExtractJsonString(
            attachResponse,
            "sessionId");

    if (sessionId.empty())
    {
        std::cerr
            << "ERROR: sessionId not found\n"
            << attachResponse
            << "\n";

        return 1;
    }

    std::cout
        << "Target attached\n";

    // ========================================================
    // 5. Enable Page / Runtime
    // ========================================================

    std::string response;

    SendCommand(
        ws,
        3,
        "Page.enable",
        "{}",
        sessionId,
        response);

    SendCommand(
        ws,
        4,
        "Runtime.enable",
        "{}",
        sessionId,
        response);

    // ========================================================
    // 6. Financial
    // ========================================================

    int commandId = 10;

    std::string financialJson;

    if (!FetchJson(
            ws,
            sessionId,
            commandId++,
            financialUrl,
            financialJson))
    {
        std::cerr
            << "ERROR: Cannot read financial JSON\n";

        return 1;
    }

    std::cout
        << "FINANCIAL JSON : OK\n"
        << "Bytes          : "
        << financialJson.size()
        << "\n";

    // ========================================================
    // 7. Highlight
    // ========================================================

    std::string highlightJson;

    if (!FetchJson(
            ws,
            sessionId,
            commandId++,
            highlightUrl,
            highlightJson))
    {
        std::cerr
            << "ERROR: Cannot read highlight JSON\n";

        return 1;
    }

    std::cout
        << "HIGHLIGHT JSON : OK\n"
        << "Bytes          : "
        << highlightJson.size()
        << "\n";
        // ========================================================
// 8. Historical Trading
// ========================================================

std::string historicalJson;

if (!FetchJson(
        ws,
        sessionId,
        commandId++,
        historicalUrl,
        historicalJson))
{
    std::cerr
        << "ERROR: Cannot read historical trading JSON\n";

    return 1;
}

std::cout
    << "HISTORICAL JSON : OK\n"
    << "Bytes           : "
    << historicalJson.size()
    << "\n";

    // ========================================================
    // 8. Save
    // ========================================================

    const fs::path outputDir =
        fs::path("Data") /
        "Fundamental" /
        "JSON";

    fs::create_directories(
        outputDir);

    const fs::path financialFile =
        outputDir /
        (symbol + "_financial_data.json");

    const fs::path highlightFile =
        outputDir /
        (symbol + "_highlight_data.json");

    {
        std::ofstream out(
            financialFile,
            std::ios::binary);

        out << financialJson;
    }

    {
        std::ofstream out(
            highlightFile,
            std::ios::binary);

        out << highlightJson;
    }

    // ========================================================
    // 9. Result
    // ========================================================

    std::cout
        << "\n========================================\n"
        << "RESULT\n"
        << "========================================\n"
        << "FINANCIAL : "
        << financialFile
        << "\n"
        << "HIGHLIGHT : "
        << highlightFile
        << "\n"
        << "Financial bytes : "
        << financialJson.size()
        << "\n"
        << "Highlight bytes : "
        << highlightJson.size()
        << "\n"
        << "========================================\n"
        << "STEP 1 PASSED\n"
        << "========================================\n";

    ws.close(
        websocket::close_code::normal);

    return 0;
}
