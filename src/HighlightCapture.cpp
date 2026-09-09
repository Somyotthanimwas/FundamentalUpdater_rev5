#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

namespace fs = std::filesystem;
namespace asio = boost::asio;
namespace websocket = boost::beast::websocket;
using tcp = asio::ip::tcp;

// ============================================================
// อ่านไฟล์ทั้งหมด
// ============================================================

static bool ReadFile(
    const fs::path& file,
    std::string& text)
{
    std::ifstream in(file, std::ios::binary);

    if (!in)
        return false;

    text.assign(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());

    return !text.empty();
}

// ============================================================
// Extract JSON string แบบง่าย
// ============================================================

static std::string ExtractJsonString(
    const std::string& json,
    const std::string& key)
{
    const std::string marker =
        "\"" + key + "\":\"";

    std::size_t p =
        json.find(marker);

    if (p == std::string::npos)
        return "";

    p += marker.size();

    std::size_t e = p;

    while (e < json.size())
    {
        if (json[e] == '"' &&
            (e == p || json[e - 1] != '\\'))
            break;

        ++e;
    }

    if (e >= json.size())
        return "";

    return json.substr(
        p,
        e - p);
}

// ============================================================
// Parse WS URL
// ============================================================

static bool ParseWsUrl(
    const std::string& url,
    std::string& host,
    std::string& port,
    std::string& target)
{
    const std::string prefix =
        "ws://";

    if (url.rfind(prefix, 0) != 0)
        return false;

    std::string s =
        url.substr(prefix.size());

    std::size_t slash =
        s.find('/');

    if (slash == std::string::npos)
        return false;

    std::string hp =
        s.substr(0, slash);

    target =
        s.substr(slash);

    std::size_t colon =
        hp.find(':');

    if (colon == std::string::npos)
        return false;

    host =
        hp.substr(0, colon);

    port =
        hp.substr(colon + 1);

    return true;
}

// ============================================================
// Send CDP command
// ============================================================

static bool SendCommand(
    websocket::stream<tcp::socket>& ws,
    int id,
    const std::string& method,
    const std::string& params,
    const std::string& sessionId,
    std::string& response)
{
    std::string message =
        "{"
        "\"id\":" +
        std::to_string(id) +
        ","
        "\"method\":\"" +
        method +
        "\","
        "\"params\":" +
        params;

    if (!sessionId.empty())
    {
        message +=
            ",\"sessionId\":\"" +
            sessionId +
            "\"";
    }

    message += "}";

    ws.write(
        asio::buffer(message));

    boost::beast::flat_buffer buffer;

    ws.read(buffer);

    response =
        boost::beast::buffers_to_string(
            buffer.data());

    return true;
}

// ============================================================
// Read page body
// ใช้กลไกเดียวกับ FundamentalCaptureTest
// ============================================================

static bool ReadPageBody(
    websocket::stream<tcp::socket>& ws,
    const std::string& sessionId,
    int& commandId,
    const std::string& url,
    std::string& result)
{
    std::string response;

    const std::string navigateParams =
        "{"
        "\"url\":\"" +
        url +
        "\""
        "}";

    if (!SendCommand(
            ws,
            commandId++,
            "Page.navigate",
            navigateParams,
            sessionId,
            response))
    {
        return false;
    }

    // รอ SET โหลด
    for (int i = 0; i < 20; ++i)
    {
        asio::steady_timer timer(
            ws.get_executor(),
            std::chrono::milliseconds(500));

        timer.wait();
    }

    const std::string evalParams =
        "{"
        "\"expression\":\"document.body.innerText\","
        "\"returnByValue\":true"
        "}";

    if (!SendCommand(
            ws,
            commandId++,
            "Runtime.evaluate",
            evalParams,
            sessionId,
            response))
    {
        return false;
    }

    const std::string marker =
        "\"value\":\"";

    std::size_t p =
        response.find(marker);

    if (p == std::string::npos)
        return false;

    p += marker.size();

    std::string value;

    bool escape = false;

    for (std::size_t i = p;
         i < response.size();
         ++i)
    {
        char c =
            response[i];

        if (escape)
        {
            switch (c)
            {
                case 'n':
                    value += '\n';
                    break;

                case 'r':
                    value += '\r';
                    break;

                case 't':
                    value += '\t';
                    break;

                case '"':
                    value += '"';
                    break;

                case '\\':
                    value += '\\';
                    break;

                default:
                    value += c;
                    break;
            }

            escape = false;
            continue;
        }

        if (c == '\\')
        {
            escape = true;
            continue;
        }

        if (c == '"')
            break;

        value += c;
    }

    if (value.empty())
        return false;

    result = value;

    return true;
}

// ============================================================
// Main
// ============================================================

int main()
{
    std::cout
        << "========================================\n"
        << "SET HIGHLIGHT CAPTURE - ALL SYMBOLS\n"
        << "========================================\n";

    // --------------------------------------------------------
    // CDP endpoints
    // --------------------------------------------------------

    std::vector<std::pair<std::string,std::string>> endpoints = {
        {"127.0.0.1", "9222"},
        {"172.31.96.1", "9223"}
    };

    std::string browserWs;

    for (const auto& ep : endpoints)
    {
        std::cout
            << "Trying CDP : http://"
            << ep.first
            << ":"
            << ep.second
            << "\n";

        // ใช้ curl command เพื่ออ่าน /json/version
        std::string cmd =
            "curl -s --max-time 3 "
            "http://" +
            ep.first +
            ":" +
            ep.second +
            "/json/version";

        FILE* pipe =
            popen(cmd.c_str(), "r");

        if (!pipe)
            continue;

        char buffer[4096];

        std::string json;

        while (fgets(
            buffer,
            sizeof(buffer),
            pipe))
        {
            json += buffer;
        }

        pclose(pipe);

        browserWs =
            ExtractJsonString(
                json,
                "webSocketDebuggerUrl");

        if (!browserWs.empty())
        {
            std::cout
                << "CDP OK : http://"
                << ep.first
                << ":"
                << ep.second
                << "\n";

            break;
        }
    }

    if (browserWs.empty())
    {
        std::cerr
            << "ERROR: Chrome CDP unavailable\n";

        return 1;
    }

    std::cout
        << "Browser WS : "
        << browserWs
        << "\n";

    // --------------------------------------------------------
    // Browser WebSocket
    // --------------------------------------------------------

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
            << "ERROR: Invalid Browser WS\n";

        return 1;
    }

    asio::io_context ioc;

    tcp::resolver resolver(ioc);

    websocket::stream<tcp::socket> ws(ioc);

    try
    {
        auto resolved =
            resolver.resolve(
                host,
                port);

        asio::connect(
            ws.next_layer(),
            resolved);

        ws.handshake(
            host + ":" + port,
            target);
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "ERROR: Browser WebSocket connect failed: "
            << e.what()
            << "\n";

        return 1;
    }

    std::cout
        << "Browser WebSocket : CONNECTED\n";

    // --------------------------------------------------------
    // Create target
    // --------------------------------------------------------

    std::string response;

    if (!SendCommand(
            ws,
            1,
            "Target.createTarget",
            "{\"url\":\"about:blank\"}",
            "",
            response))
    {
        std::cerr
            << "ERROR: Cannot create target\n";

        return 1;
    }

    std::string targetId =
        ExtractJsonString(
            response,
            "targetId");

    if (targetId.empty())
    {
        std::cerr
            << "ERROR: targetId not found\n";

        return 1;
    }

    std::cout
        << "Target ID : "
        << targetId
        << "\n";

    // --------------------------------------------------------
    // Attach
    // --------------------------------------------------------

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
            response))
    {
        std::cerr
            << "ERROR: Cannot attach target\n";

        return 1;
    }

    std::string sessionId =
        ExtractJsonString(
            response,
            "sessionId");

    if (sessionId.empty())
    {
        std::cerr
            << "ERROR: sessionId not found\n";

        return 1;
    }

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

    // --------------------------------------------------------
    // Symbols
    // --------------------------------------------------------

    std::ifstream symbolsIn(
        "symbols.txt");

    if (!symbolsIn)
    {
        std::cerr
            << "ERROR: symbols.txt not found\n";

        return 1;
    }

    std::vector<std::string> symbols;

    std::string symbol;

    while (std::getline(
        symbolsIn,
        symbol))
    {
        if (!symbol.empty())
            symbols.push_back(symbol);
    }

    std::cout
        << "Symbols : "
        << symbols.size()
        << "\n";

    // --------------------------------------------------------
    // Output
    // --------------------------------------------------------

    const fs::path outputDir =
        fs::path("Data") /
        "Fundamental" /
        "JSON";

    fs::create_directories(
        outputDir);

    int ok = 0;
    int fail = 0;

    int commandId = 10;

    // --------------------------------------------------------
    // Capture
    // --------------------------------------------------------

    for (const auto& s : symbols)
    {
        std::cout
            << "["
            << (ok + fail + 1)
            << "/"
            << symbols.size()
            << "] "
            << s
            << " ... ";

        const std::string url =
            "https://www.set.or.th/api/set/stock/" +
            s +
            "/highlight-data?lang=th";

        std::string json;

        if (!ReadPageBody(
                ws,
                sessionId,
                commandId,
                url,
                json))
        {
            std::cout
                << "FAIL\n";

            ++fail;
            continue;
        }

        const fs::path file =
            outputDir /
            (s + "_highlight_data.json");

        std::ofstream out(
            file,
            std::ios::binary);

        if (!out)
        {
            std::cout
                << "SAVE FAIL\n";

            ++fail;
            continue;
        }

        out << json;
        out.close();

        std::cout
            << "OK "
            << json.size()
            << " bytes\n";

        ++ok;
    }

    ws.close(
        websocket::close_code::normal);

    // --------------------------------------------------------
    // Result
    // --------------------------------------------------------

    std::cout
        << "\n========================================\n"
        << "SET HIGHLIGHT RESULT\n"
        << "========================================\n"
        << "Symbols : "
        << symbols.size()
        << "\n"
        << "OK      : "
        << ok
        << "\n"
        << "Failed  : "
        << fail
        << "\n"
        << "Output  : "
        << outputDir
        << "\n"
        << "========================================\n";

    return fail == 0 ? 0 : 1;
}
