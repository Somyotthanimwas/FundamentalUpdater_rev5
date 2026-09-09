#include <algorithm>
#include <cstdlib>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <curl/curl.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>
#include <thread>
#include <chrono>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;

using tcp = asio::ip::tcp;


// ============================================================
// HTTP GET
// ============================================================

static size_t WriteCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userp)
{
    const size_t total = size * nmemb;

    std::string* output =
        static_cast<std::string*>(userp);

    output->append(
        static_cast<char*>(contents),
        total);

    return total;
}


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
        10L);

    CURLcode result =
        curl_easy_perform(curl);

    long httpCode = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &httpCode);

    curl_easy_cleanup(curl);

    if (result != CURLE_OK)
{
    std::cerr
        << "CURL ERROR : "
        << curl_easy_strerror(result)
        << "\n";
}

std::cerr
    << "HTTP CODE  : "
    << httpCode
    << "\n";

std::cerr
    << "RESP SIZE  : "
    << response.size()
    << "\n";

if (!response.empty())
{
    std::cerr
        << "RESP HEAD  : "
        << response.substr(
            0,
            std::min<size_t>(
                response.size(),
                200))
        << "\n";
}

return result == CURLE_OK &&
       httpCode == 200 &&
       !response.empty();
}


// ============================================================
// EXTRACT STRING FROM JSON
// ============================================================

static std::string ExtractJsonString(
    const std::string& json,
    const std::string& key)
{
    const std::string marker =
        "\"" + key + "\"";

    size_t p =
        json.find(marker);

    if (p == std::string::npos)
        return "";

    p += marker.size();

    while (
        p < json.size() &&
        (
            json[p] == ' ' ||
            json[p] == '\t' ||
            json[p] == '\r' ||
            json[p] == '\n'
        ))
    {
        ++p;
    }

    if (
        p >= json.size() ||
        json[p] != ':')
    {
        return "";
    }

    ++p;

    while (
        p < json.size() &&
        (
            json[p] == ' ' ||
            json[p] == '\t' ||
            json[p] == '\r' ||
            json[p] == '\n'
        ))
    {
        ++p;
    }

    if (
        p >= json.size() ||
        json[p] != '"')
    {
        return "";
    }

    ++p;

    std::string value;

    bool escaped = false;

    for (
        size_t i = p;
        i < json.size();
        ++i)
    {
        const char c = json[i];

        if (escaped)
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

                case '/':
                    value += '/';
                    break;

                default:
                    value += c;
                    break;
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
    std::string& result)
{
    std::ostringstream msg;

    msg
        << "{"
        << "\"id\":"
        << id
        << ","
        << "\"method\":\""
        << method
        << "\"";

    if (!params.empty())
    {
        msg
            << ","
            << "\"params\":"
            << params;
    }

    msg << "}";

    ws.write(
        asio::buffer(
            msg.str()));

    for (;;)
    {
        beast::flat_buffer buffer;

        ws.read(buffer);

        const std::string response =
            beast::buffers_to_string(
                buffer.data());

        const std::string idText =
            "\"id\":" +
            std::to_string(id);

        if (
            response.find(idText)
            != std::string::npos)
        {
            result = response;

            return true;
        }
    }
}


// ============================================================
// EXTRACT CDP RETURN VALUE
// ============================================================

static bool ExtractValue(
    const std::string& response,
    std::string& value)
{
    const std::string valueKey =
        "\"value\":\"";

    const size_t valuePos =
        response.find(valueKey);

    if (valuePos == std::string::npos)
        return false;

    const size_t start =
        valuePos +
        valueKey.size();

    bool escaped = false;

    value.clear();

    for (
        size_t i = start;
        i < response.size();
        ++i)
    {
        const char c = response[i];

        if (escaped)
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

                case '/':
                    value += '/';
                    break;

                default:
                    value += c;
                    break;
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

    return !value.empty();
}


// ============================================================
// FETCH SET JSON THROUGH CHROME
// ============================================================

static bool FetchJson(
    websocket::stream<tcp::socket>& ws,
    int commandId,
    const std::string& url,
    std::string& output)
{
    constexpr int MAX_RETRIES = 3;
    constexpr int RETRY_DELAY_MS = 2000;

    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt)
    {
        if (attempt > 1)
        {
            std::cout
                << "Retry "
                << attempt
                << "/"
                << MAX_RETRIES
                << " ... waiting "
                << RETRY_DELAY_MS
                << " ms\n";

            std::this_thread::sleep_for(
                std::chrono::milliseconds(RETRY_DELAY_MS));
        }

        std::cout
            << "\nFetching"
            << (attempt > 1 ? " (retry)" : "")
            << ":\n"
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
                commandId + attempt - 1,
                "Runtime.evaluate",
                params,
                response))
        {
            if (attempt == MAX_RETRIES)
                return false;

            continue;
        }

        if (ExtractValue(
                response,
                output))
        {
            return true;
        }

        std::cerr
            << "ERROR: Cannot extract fetch value";

        if (attempt < MAX_RETRIES)
        {
            std::cerr
                << " - will retry\n";
        }
        else
        {
            std::cerr
                << "\n"
                << response.substr(
                    0,
                    std::min<size_t>(
                        response.size(),
                        1000))
                << "\n";
        }
    }

    return false;
}


// ============================================================
// SAVE JSON
// ============================================================

static bool SaveJson(
    const fs::path& file,
    const std::string& json)
{
    std::ofstream out(
        file,
        std::ios::binary);

    if (!out)
        return false;

    out << json;

    return true;
}


// ============================================================
// CAPTURE ONE SYMBOL
// ============================================================

static bool CaptureSymbol(
    websocket::stream<tcp::socket>& ws,
    const std::string& symbol,
    const fs::path& projectDir)
{
    // --------------------------------------------------------
    // API #1 Financial
    // --------------------------------------------------------

    const std::string financialUrl =
        "https://www.set.or.th/api/set/stock/" +
        symbol +
        "/company-highlight/financial-data?lang=th";

    // --------------------------------------------------------
    // API #2 Trading Stat
    // --------------------------------------------------------

    const std::string tradingUrl =
        "https://www.set.or.th/api/set/stock/" +
        symbol +
        "/highlight-data?lang=th";

    // --------------------------------------------------------
    // API #3 Historical Trading
    // --------------------------------------------------------

    const std::string historicalUrl =
        "https://www.set.or.th/api/set/stock/" +
        symbol +
        "/historical-trading?lang=th";

    // --------------------------------------------------------
    // API #4 Current Market Data
    // --------------------------------------------------------

    const std::string relatedProductUrl =
        "https://www.set.or.th/api/set/stock/" +
        symbol +
        "/related-product/o?lang=th";


    std::string financialJson;
    std::string tradingJson;
    std::string historicalJson;
    std::string relatedProductJson;


    std::cout
        << "\n----------------------------------------\n"
        << "SYMBOL : "
        << symbol
        << "\n"
        << "----------------------------------------\n";


    // ========================================================
    // FINANCIAL
    // ========================================================

    std::cout
        << "Financial ... ";

    if (!FetchJson(
            ws,
            100,
            financialUrl,
            financialJson))
    {
        std::cout
            << "FAILED\n";

        return false;
    }

    std::cout
        << "OK ("
        << financialJson.size()
        << " bytes)\n";


    // ========================================================
    // TRADING STAT
    // ========================================================

    std::cout
        << "Trading   ... ";

    if (!FetchJson(
            ws,
            200,
            tradingUrl,
            tradingJson))
    {
        std::cout
            << "FAILED\n";

        return false;
    }

    std::cout
        << "OK ("
        << tradingJson.size()
        << " bytes)\n";


    // ========================================================
    // HISTORICAL TRADING
    // ========================================================

    std::cout
        << "Historical... ";

    if (!FetchJson(
            ws,
            300,
            historicalUrl,
            historicalJson))
    {
        std::cout
            << "FAILED\n";

        return false;
    }

    std::cout
        << "OK ("
        << historicalJson.size()
        << " bytes)\n";


    // ========================================================
    // CURRENT MARKET DATA
    // related-product/o
    // ========================================================

    std::cout
        << "Current   ... ";

    if (!FetchJson(
            ws,
            400,
            relatedProductUrl,
            relatedProductJson))
    {
        std::cout
            << "FAILED\n";

        return false;
    }

    std::cout
        << "OK ("
        << relatedProductJson.size()
        << " bytes)\n";


    // ========================================================
    // OUTPUT DIRECTORY
    // ========================================================

      fs::path outputBasePath = projectDir;

      // FundamentalCapture.exe is a Windows MinGW executable.
      // Convert WSL mounted path /mnt/c/... to C:/...
      if (projectDir.string().size() >= 7 &&
          projectDir.string().rfind("/mnt/", 0) == 0 &&
          projectDir.string()[5] >= 'a' &&
          projectDir.string()[5] <= 'z' &&
          projectDir.string()[6] == '/')
      {
          std::string windowsProjectPath =
              projectDir.string().substr(5, 1);

          windowsProjectPath[0] =
              static_cast<char>(
                  windowsProjectPath[0] - 'a' + 'A');

          windowsProjectPath += ":";
          windowsProjectPath +=
              projectDir.string().substr(6);

          outputBasePath =
              fs::path(windowsProjectPath);
      }

      const fs::path outputDir =
          outputBasePath
          / "Data"
          / "Fundamental"
          / "JSON";

    fs::create_directories(
        outputDir);


    // ========================================================
    // OUTPUT FILES
    // ========================================================

    const fs::path financialFile =
        outputDir /
        (symbol +
         "_financial_data.json");

    const fs::path tradingFile =
        outputDir /
        (symbol +
         "_trading_stat.json");

    const fs::path historicalFile =
        outputDir /
        (symbol +
         "_historical_trading.json");

    const fs::path relatedProductFile =
        outputDir /
        (symbol +
         "_related_product.json");


    // ========================================================
    // SAVE FINANCIAL
    // ========================================================

    if (!SaveJson(
            financialFile,
            financialJson))
    {
        std::cerr
            << "ERROR: Cannot save Financial\n";

        return false;
    }


    // ========================================================
    // SAVE TRADING
    // ========================================================

    if (!SaveJson(
            tradingFile,
            tradingJson))
    {
        std::cerr
            << "ERROR: Cannot save Trading\n";

        return false;
    }


    // ========================================================
    // SAVE HISTORICAL
    // ========================================================

    if (!SaveJson(
            historicalFile,
            historicalJson))
    {
        std::cerr
            << "ERROR: Cannot save Historical\n";

        return false;
    }


    // ========================================================
    // SAVE CURRENT MARKET DATA
    // ========================================================

    if (!SaveJson(
            relatedProductFile,
            relatedProductJson))
    {
        std::cerr
            << "ERROR: Cannot save Current Market Data\n";

        return false;
    }


    // ========================================================
    // RESULT
    // ========================================================

    std::cout
        << "Saved Financial : "
        << financialFile
        << "\n";

    std::cout
        << "Saved Trading   : "
        << tradingFile
        << "\n";

    std::cout
        << "Saved Historical: "
        << historicalFile
        << "\n";

    std::cout
        << "Saved Current   : "
        << relatedProductFile
        << "\n";


    return true;
}
// ============================================================
// START CHROME CDP
// ============================================================

static bool StartChromeCDP()
{
#ifdef _WIN32

    const std::string chromePath =
        R"(C:\Program Files\Google\Chrome\Application\chrome.exe)";

    const char* tempDir = std::getenv("TEMP");

    const std::string profileDir =
        std::string(tempDir ? tempDir : "C:\\Windows\\Temp") +
        R"(\FundamentalUpdater_rev5_ChromeProfile)";

    const std::string url =
        "https://www.set.or.th/th/market/product/stock/quote/AOT/price";

    std::string command =
        "\"" + chromePath + "\""
        " --remote-debugging-port=9222"
        " --user-data-dir=\"" + profileDir + "\""
        " --headless=new"
        " --no-first-run"
        " --no-default-browser-check"
        " \"" + url + "\"";

    std::cout
        << "Starting Chrome CDP...\n";

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    // Run Chrome completely hidden.
    // CDP on port 9222 continues to work normally.
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<char> cmd(
        command.begin(),
        command.end());

    cmd.push_back('\0');

    BOOL ok =
        CreateProcessA(
            nullptr,
            cmd.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi);

    if (!ok)
    {
        std::cerr
            << "ERROR: Cannot start Chrome\n";

        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return true;

#else

    return false;

#endif
}


// ============================================================
// WAIT FOR CHROME CDP
// ============================================================

static bool WaitForChromeCDP(
    const std::string& cdp,
    int timeoutSeconds)
{
    std::string version;

    for (int i = 0;
         i < timeoutSeconds * 10;
         ++i)
    {
        if (HttpGet(
                cdp + "/json/version",
                version))
        {
            return true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));
    }

    return false;
}

// ============================================================
// MAIN
// ============================================================

int main(int argc, char* argv[])
{
    std::cout
        << "========================================\n"
        << "C++ SET FUNDAMENTAL REAL CAPTURE\n"
        << "========================================\n";


    // ========================================================
    // CDP
    // ========================================================

    const std::string cdp =
    "http://127.0.0.1:9222";

std::string version;

if (!HttpGet(
        cdp + "/json/version",
        version))
{
    std::cout
        << "Chrome CDP not running.\n";

    if (!StartChromeCDP())
    {
        std::cerr
            << "ERROR: Cannot start Chrome CDP\n";

        return 1;
    }

    std::cout
        << "Waiting for Chrome CDP...\n";

    if (!WaitForChromeCDP(
            cdp,
            20))
    {
        std::cerr
            << "ERROR: Chrome CDP unavailable\n";

        return 1;
    }

    std::cout
        << "Chrome CDP started : OK\n";
}

std::cout
    << "Chrome CDP : OK\n";

    // ========================================================
    // GET TABS
    // ========================================================

    std::string tabs;

    if (!HttpGet(
            cdp + "/json",
            tabs))
    {
        std::cerr
            << "ERROR: Cannot get Chrome tabs\n";

        return 1;
    }

    std::cout
        << "Chrome tabs : OK\n";

    std::cout
        << "===== CHROME TABS JSON =====\n"
        << tabs
        << "\n===== END CHROME TABS JSON =====\n";


    // ========================================================
    // FIND SET PRICE PAGE
    //
    // Use the existing SET Stock Quote / Price tab as the
    // Chrome CDP execution context for SET API fetch().
    // Do NOT require company-highlights page to be open.
    // ========================================================

    const std::string marker =
        "/market/product/stock/quote/";

    const size_t markerPos =
        tabs.find(marker);

    if (markerPos == std::string::npos)
    {
        std::cerr
            << "\nERROR: SET stock quote page not found.\n";

        return 1;
    }


    const size_t pageStart =
        tabs.rfind(
            "{",
            markerPos);

    const size_t pageEnd =
        tabs.find(
            "}",
            markerPos);

    if (
        pageStart == std::string::npos ||
        pageEnd == std::string::npos)
    {
        std::cerr
            << "ERROR: Cannot locate SET tab object\n";

        return 1;
    }


    const std::string page =
        tabs.substr(
            pageStart,
            pageEnd -
            pageStart +
            1);


    // ========================================================
    // WEBSOCKET URL
    // ========================================================

    const std::string wsMarker =
        "\"webSocketDebuggerUrl\"";

    const size_t wsPos =
        page.find(wsMarker);

    if (wsPos == std::string::npos)
    {
        std::cerr
            << "ERROR: websocketDebuggerUrl not found\n";

        return 1;
    }


    const size_t colonPos =
        page.find(
            ':',
            wsPos +
            wsMarker.size());

    if (colonPos == std::string::npos)
    {
        std::cerr
            << "ERROR: Invalid websocketDebuggerUrl\n";

        return 1;
    }


    const size_t quoteStart =
        page.find(
            '"',
            colonPos + 1);

    if (quoteStart == std::string::npos)
    {
        std::cerr
            << "ERROR: Invalid websocket URL\n";

        return 1;
    }


    const size_t wsStart =
        quoteStart + 1;

    const size_t wsEnd =
        page.find(
            '"',
            wsStart);

    if (wsEnd == std::string::npos)
    {
        std::cerr
            << "ERROR: Invalid websocket URL\n";

        return 1;
    }


    std::string wsUrl =
        page.substr(
            wsStart,
            wsEnd -
            wsStart);


    // ========================================================
    // CHROME -> WSL
    // ========================================================

    const std::string chromeWs =
        "ws://127.0.0.1:9222";


    // Running directly on Windows:
    // keep Chrome CDP WebSocket URL unchanged.



    std::cout
        << "WebSocket : "
        << wsUrl
        << "\n";


    // ========================================================
    // PARSE WS
    // ========================================================

    const std::string prefix =
        "ws://";

    if (
        wsUrl.rfind(
            prefix,
            0) != 0)
    {
        std::cerr
            << "ERROR: Unsupported WebSocket URL\n";

        return 1;
    }


    const std::string wsAddress =
        wsUrl.substr(
            prefix.size());

    const size_t slash =
        wsAddress.find('/');

    if (slash == std::string::npos)
    {
        std::cerr
            << "ERROR: Invalid WebSocket address\n";

        return 1;
    }


    const std::string hostPort =
        wsAddress.substr(
            0,
            slash);

    const std::string target =
        wsAddress.substr(
            slash);


    const size_t colon =
        hostPort.rfind(':');

    if (colon == std::string::npos)
    {
        std::cerr
            << "ERROR: Invalid host:port\n";

        return 1;
    }


    const std::string host =
        hostPort.substr(
            0,
            colon);

    const std::string port =
        hostPort.substr(
            colon + 1);


    // ========================================================
    // CONNECT
    // ========================================================

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
        hostPort,
        target);


    std::cout
        << "CDP WebSocket : CONNECTED\n";


    // ========================================================
    // LOAD SYMBOLS
    // ========================================================

    // Fundamental V4 project root.
    //
    // Application.exe may pass a WSL UNC path such as:
    //
    //   /wsl.localhost/Ubuntu/home/thanimwas/FundamentalUpdater_rev4
    //
    // That path is NOT a real Linux filesystem path.
    // Convert it to:
    //
    //   /home/thanimwas/FundamentalUpdater_rev4
    //
    // before opening symbols.txt and Data files.

    fs::path projectDir;

    if (argc >= 2 && argv[1] && *argv[1])
    {
        std::string projectPathArg = argv[1];

        const std::string prefix1 =
            "/wsl.localhost/Ubuntu/";

        const std::string prefix2 =
            "/wsl$/Ubuntu/";

        if (projectPathArg.rfind(prefix1, 0) == 0)
        {
            projectPathArg =
                "/" +
                projectPathArg.substr(prefix1.size());
        }
        else if (projectPathArg.rfind(prefix2, 0) == 0)
        {
            projectPathArg =
                "/" +
                projectPathArg.substr(prefix2.size());
        }

        for (char& c : projectPathArg)
        {
            if (c == '\\')
                c = '/';
        }

        projectDir =
            fs::path(projectPathArg);
    }
    else
    {
        projectDir =
            fs::current_path();
    }

    std::string projectPath =
        projectDir.string();

    // Normalize path separators for WSL/Linux.
    for (char& c : projectPath)
    {
        if (c == '\\')
            c = '/';
    }

    while (
        projectPath.size() > 1 &&
        projectPath.back() == '/')
    {
        projectPath.pop_back();
    }

    const fs::path symbolsFile =
        fs::path(projectPath + "/symbols.txt");

    std::cout
        << "Project directory : "
        << projectPath
        << "\n"
        << "Symbols file      : "
        << symbolsFile
        << "\n";

    // ========================================================
    // WINDOWS EXE PATH CONVERSION
    //
    // FundamentalCapture is a Windows MinGW executable.
    // WSL paths such as:
    //
    //   /mnt/c/FundamentalUpdater_rev4/symbols.txt
    //
    // are valid to WSL shell tools but are not native Windows
    // filesystem paths for the Windows executable.
    //
    // Convert /mnt/c/... -> C:/...
    // ========================================================

    fs::path symbolsOpenPath = symbolsFile;

    if (projectPath.size() >= 6 &&
        projectPath.rfind("/mnt/", 0) == 0 &&
        projectPath[5] >= 'a' &&
        projectPath[5] <= 'z' &&
        projectPath[6] == '/')
    {
        std::string windowsPath =
            projectPath.substr(5, 1);

        windowsPath[0] =
            static_cast<char>(
                windowsPath[0] - 'a' + 'A');

        windowsPath += ":";
        windowsPath += projectPath.substr(6);
        windowsPath += "/symbols.txt";

        symbolsOpenPath =
            fs::path(windowsPath);

        std::cout
            << "Windows symbols path: "
            << symbolsOpenPath
            << "\n";
    }

    std::vector<std::string> symbols;


    {
        std::ifstream in(
            symbolsOpenPath);

        if (!in)
        {
            std::cerr
                << "ERROR: Cannot open symbols.txt\n"
                << "Attempted path: "
                << symbolsOpenPath
                << "\n";

            ws.close(
                websocket::close_code::normal);

            return 1;
        }


        std::string symbol;


        while (
            std::getline(
                in,
                symbol))
        {
            if (!symbol.empty())
                symbols.push_back(symbol);
        }
    }


    std::cout
        << "Symbols loaded : "
        << symbols.size()
        << "\n";


    if (symbols.empty())
    {
        std::cerr
            << "ERROR: symbols.txt is empty\n";

        ws.close(
            websocket::close_code::normal);

        return 1;
    }

       // ========================================================
// FRESH JSON DIRECTORY
// ลบ JSON รอบเก่าทั้งหมดทุกครั้งก่อน Capture
// ========================================================

  fs::path jsonBasePath = projectDir;

  if (projectPath.size() >= 7 &&
      projectPath.rfind("/mnt/", 0) == 0 &&
      projectPath[5] >= 'a' &&
      projectPath[5] <= 'z' &&
      projectPath[6] == '/')
  {
      std::string windowsProjectPath =
          projectPath.substr(5, 1);

      windowsProjectPath[0] =
          static_cast<char>(
              windowsProjectPath[0] - 'a' + 'A');

      windowsProjectPath += ":";
      windowsProjectPath += projectPath.substr(6);

      jsonBasePath =
          fs::path(windowsProjectPath);

      std::cout
          << "Windows project path: "
          << jsonBasePath
          << "\n";
  }

  const fs::path jsonDir =
      jsonBasePath /
      "Data" /
      "Fundamental" /
      "JSON";

std::cout
    << "\n========================================\n"
    << "PREPARE FRESH FUNDAMENTAL JSON\n"
    << "========================================\n"
    << "Directory : "
    << jsonDir
    << "\n";

  try
  {
      // Keep the JSON directory on Windows/WSL mounted filesystem.
      // Remove only old JSON files inside it.
      if (!fs::exists(jsonDir))
      {
          fs::create_directories(jsonDir);

          std::cout
              << "JSON directory created\n";
      }
      else
      {
          for (const auto& entry :
               fs::directory_iterator(jsonDir))
          {
              if (!entry.is_regular_file())
                  continue;

              if (entry.path().extension() == ".json")
              {
                  std::error_code ec;

                  fs::remove(
                      entry.path(),
                      ec);

                  if (ec)
                  {
                      std::cerr
                          << "WARNING: Cannot remove old JSON: "
                          << entry.path()
                          << " : "
                          << ec.message()
                          << "\n";
                  }
              }
          }

          std::cout
              << "Old JSON files removed\n";
      }
  }
  catch (const std::exception& e)
  {
      std::cerr
          << "ERROR: Cannot prepare JSON directory\n"
          << e.what()
          << "\n";

      ws.close(
          websocket::close_code::normal);

      return 1;
  }

    // ========================================================
    // CAPTURE ALL
    // ========================================================

    int success = 0;
    int failed = 0;


    for (
        const auto& symbol :
        symbols)
    {
        if (
            CaptureSymbol(
                ws,
                symbol,
                projectDir))
        {
            ++success;
        }
        else
        {
            ++failed;
        }
    }


    // ========================================================
    // RESULT
    // ========================================================

    std::cout
        << "\n========================================\n"
        << "CAPTURE TEST RESULT\n"
        << "========================================\n"
        << "Success : "
        << success
        << "\n"
        << "Failed  : "
        << failed
        << "\n"
        << "========================================\n";


    ws.close(
        websocket::close_code::normal);


    return failed == 0
        ? 0
        : 1;
}
