#include "PriceUpdater.h"
#include <map>
#include "PriceToAmiBroker.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <cctype>
#include <filesystem>

#include <curl/curl.h>

namespace fs = std::filesystem;
#ifdef _WIN32
#include <windows.h>
#endif

struct EasyContext
{
    std::string symbol;
    std::string response;
    CURL* easy = nullptr;
};

static size_t WriteCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userp)
{
    size_t total = size * nmemb;

    auto* output =
        static_cast<std::string*>(userp);

    output->append(
        static_cast<char*>(contents),
        total);

    return total;
}

static std::string GetTimestamp()
{
    std::time_t now =
        std::time(nullptr);

    std::tm tm{};

#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif

    std::ostringstream oss;

    oss << std::put_time(
        &tm,
        "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

static std::string ExtractMarketDate(
    const std::string& response)
{
    /*
     * Use the actual SET quote update date.
     *
     * HTML:
     * ข้อมูลล่าสุด :
     * <span>01 ก.ย. 2569 12:19:32</span>
     *
     * This is the trading/quote date.
     * Do NOT use statisticsAsOf.
     */

    const std::string marker =
        "ข้อมูลล่าสุด";

    size_t pos =
        response.find(marker);

    if (pos == std::string::npos)
        return "";

    size_t spanStart =
        response.find("<span", pos);

    if (spanStart == std::string::npos)
        return "";

    spanStart =
        response.find(">", spanStart);

    if (spanStart == std::string::npos)
        return "";

    ++spanStart;

    size_t spanEnd =
        response.find("</span>", spanStart);

    if (spanEnd == std::string::npos)
        return "";

    std::string value =
        response.substr(
            spanStart,
            spanEnd - spanStart);

    while (!value.empty() &&
           std::isspace(
               static_cast<unsigned char>(
                   value.front())))
    {
        value.erase(value.begin());
    }

    while (!value.empty() &&
           std::isspace(
               static_cast<unsigned char>(
                   value.back())))
    {
        value.pop_back();
    }

    /*
     * Expected:
     *
     * DD ม.ค. YYYY HH:MM:SS
     */

    if (value.size() < 13)
        return "";

    std::string day =
        value.substr(0, 2);

    size_t monthStart = 3;
    size_t monthEnd =
        value.find(' ', monthStart);

    if (monthEnd == std::string::npos)
        return "";

    std::string month =
        value.substr(
            monthStart,
            monthEnd - monthStart);

    size_t yearStart =
        monthEnd + 1;

    if (yearStart + 4 > value.size())
        return "";

    std::string thaiYear =
        value.substr(yearStart, 4);

    static const std::map<std::string, std::string> thaiMonths = {
        {"ม.ค.", "01"},
        {"ก.พ.", "02"},
        {"มี.ค.", "03"},
        {"เม.ย.", "04"},
        {"พ.ค.", "05"},
        {"มิ.ย.", "06"},
        {"ก.ค.", "07"},
        {"ส.ค.", "08"},
        {"ก.ย.", "09"},
        {"ต.ค.", "10"},
        {"พ.ย.", "11"},
        {"ธ.ค.", "12"}
    };

    auto it =
        thaiMonths.find(month);

    if (it == thaiMonths.end())
        return "";

    int gregorianYear =
        std::stoi(thaiYear) - 543;

    std::ostringstream oss;

    oss << gregorianYear
        << "-"
        << it->second
        << "-"
        << day;

    return oss.str();
}

/*
 * SET may show the current calendar date in "ข้อมูลล่าสุด"
 * even when the latest OHLCV belongs to the previous trading day.
 *
 * Price only:
 * - Saturday -> Friday
 * - Sunday   -> Friday
 *
 * Do NOT use the computer's current date.
 * This operates only on the date returned by SET.
 */
static std::string NormalizeMarketDate(
    const std::string& date)
{
    if (date.size() != 10)
        return date;

    int year = std::stoi(date.substr(0, 4));
    int month = std::stoi(date.substr(5, 2));
    int day = std::stoi(date.substr(8, 2));

    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon  = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = 12;

    std::time_t t = std::mktime(&tm);
    if (t == static_cast<std::time_t>(-1))
        return date;

    /*
     * tm_wday:
     * 0 = Sunday
     * 6 = Saturday
     */
    if (tm.tm_wday == 6)
    {
        t -= 24 * 60 * 60;
    }
    else if (tm.tm_wday == 0)
    {
        t -= 2 * 24 * 60 * 60;
    }
    else
    {
        return date;
    }

    std::tm normalized{};
#ifdef _WIN32
    localtime_s(&normalized, &t);
#else
    localtime_r(&t, &normalized);
#endif

    std::ostringstream oss;
    oss << std::put_time(
        &normalized,
        "%Y-%m-%d");

    return oss.str();
}

static std::string ExtractLabelValue(
    const std::string& response,
    const std::string& label)
{
    // SET HTML:
    //
    // <label class="...">ราคาเปิด</label>
    // <span class="...">65.75</span>
    //
    // เดิม parser หาเฉพาะ <label>...</label>
    // ซึ่งไม่รองรับ attributes ใน <label>

    std::string marker =
        ">" + label + "</label>";

    size_t pos =
        response.find(marker);

    if (pos == std::string::npos)
        return "";

    pos += marker.size();

    size_t spanStart =
        response.find("<span", pos);

    if (spanStart == std::string::npos)
        return "";

    spanStart =
        response.find(">", spanStart);

    if (spanStart == std::string::npos)
        return "";

    ++spanStart;

    size_t spanEnd =
        response.find("</span>", spanStart);

    if (spanEnd == std::string::npos)
        return "";

    std::string value =
        response.substr(
            spanStart,
            spanEnd - spanStart);

    while (!value.empty() &&
           std::isspace(
               static_cast<unsigned char>(
                   value.front())))
    {
        value.erase(value.begin());
    }

    while (!value.empty() &&
           std::isspace(
               static_cast<unsigned char>(
                   value.back())))
    {
        value.pop_back();
    }

    return value;
}

static std::string ParseField(
    const std::string& response,
    const std::string& label)
{
    std::string value =
        ExtractLabelValue(
            response,
            label);

    if (value.empty() ||
        value == "-")
    {
        return "";
    }

    // Volume เช่น 16,412,690
    // เอา comma ออก
    value.erase(
        std::remove(
            value.begin(),
            value.end(),
            ','),
        value.end());

    return value;
}

static CURL* CreateEasy(
    EasyContext* ctx)
{
    CURL* curl =
        curl_easy_init();

    if (!curl)
        return nullptr;

    std::string url =
        "https://www.set.or.th/th/market/product/stock/quote/" +
        ctx->symbol +
        "/price";

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
        &ctx->response);

    curl_easy_setopt(
        curl,
        CURLOPT_USERAGENT,
        "Mozilla/5.0");

    curl_easy_setopt(
        curl,
        CURLOPT_FOLLOWLOCATION,
        1L);

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        30L);

    curl_easy_setopt(
        curl,
        CURLOPT_ACCEPT_ENCODING,
        "");

    curl_easy_setopt(
        curl,
        CURLOPT_PRIVATE,
        ctx);

    return curl;
}

int PriceUpdater::Run()
{
    std::cout
        << "==============================\n"
        << "SET PRICE OHLCV - PARALLEL\n"
        << "==============================\n\n";

    // Resolve symbols.txt relative to the project directory
    // instead of the process current working directory.
    fs::path projectDir;

#ifdef _WIN32
    {
        char exeBuffer[MAX_PATH]{};
        DWORD len = GetModuleFileNameA(
            nullptr,
            exeBuffer,
            MAX_PATH
        );

        if (len > 0)
        {
            fs::path exePath(
                std::string(exeBuffer, len)
            );

            projectDir = exePath.parent_path();

            // Development layout:
            //   project/build-win/FundamentalUpdater_rev4.exe
            //   project/symbols.txt
            if (!fs::exists(projectDir / "symbols.txt") &&
                fs::exists(projectDir.parent_path() / "symbols.txt"))
            {
                projectDir = projectDir.parent_path();
            }
        }
    }
#endif

    if (projectDir.empty())
    {
        projectDir = fs::current_path();
    }

    const fs::path symbolsPath =
        projectDir / "symbols.txt";

    std::cout
        << "Symbols file : "
        << symbolsPath
        << std::endl;

    std::ifstream symbolsFile(
        symbolsPath);

    if (!symbolsFile)
    {
        std::cerr
            << "ERROR: cannot open symbols.txt\n";

        return 1;
    }

    std::vector<std::string> symbols;

    std::string symbol;

    while (std::getline(
        symbolsFile,
        symbol))
    {
        if (!symbol.empty())
            symbols.push_back(symbol);
    }

    symbolsFile.close();

    if (symbols.empty())
    {
        std::cerr
            << "ERROR: symbols.txt is empty\n";

        return 1;
    }

    std::ofstream csv(
        "set-price.csv");

    if (!csv)
    {
        std::cerr
            << "ERROR: cannot create set-price.csv\n";

        return 1;
    }

    csv
        << "Symbol,Open,High,Low,Close,Volume,Timestamp,Status\n";

    curl_global_init(
        CURL_GLOBAL_DEFAULT);

    CURLM* multi =
        curl_multi_init();

    if (!multi)
    {
        std::cerr
            << "ERROR: curl_multi_init failed\n";

        curl_global_cleanup();

        return 1;
    }

    const size_t MAX_PARALLEL = 20;

    size_t completed = 0;
    size_t success = 0;
    size_t failed = 0;

    while (completed < symbols.size())
    {
        size_t batchEnd =
            std::min(
                completed + MAX_PARALLEL,
                symbols.size());

        std::vector<EasyContext> contexts(
            batchEnd - completed);

        for (size_t i = completed;
             i < batchEnd;
             ++i)
        {
            EasyContext& ctx =
                contexts[i - completed];

            ctx.symbol =
                symbols[i];

            ctx.response.clear();

            ctx.easy =
                CreateEasy(&ctx);

            if (!ctx.easy)
            {
                ++failed;

                csv
                    << ctx.symbol
                    << ",,,,,"
                    << GetTimestamp()
                    << ",HTTP_ERROR\n";

                continue;
            }

            CURLMcode mc =
                curl_multi_add_handle(
                    multi,
                    ctx.easy);

            if (mc != CURLM_OK)
            {
                ++failed;

                curl_easy_cleanup(
                    ctx.easy);

                ctx.easy = nullptr;

                csv
                    << ctx.symbol
                    << ",,,,,"
                    << GetTimestamp()
                    << ",HTTP_ERROR\n";
            }
        }

        int running = 0;

        curl_multi_perform(
            multi,
            &running);

        while (running)
        {
            int numfds = 0;

            curl_multi_wait(
                multi,
                nullptr,
                0,
                1000,
                &numfds);

            curl_multi_perform(
                multi,
                &running);

            int msgs = 0;

            CURLMsg* msg = nullptr;

            while ((msg =
                curl_multi_info_read(
                    multi,
                    &msgs)) != nullptr)
            {
                if (msg->msg != CURLMSG_DONE)
                    continue;

                CURL* easy =
                    msg->easy_handle;

                EasyContext* ctx =
                    nullptr;

                curl_easy_getinfo(
                    easy,
                    CURLINFO_PRIVATE,
                    &ctx);

                if (!ctx)
                    continue;

                long httpCode = 0;

                curl_easy_getinfo(
                    easy,
                    CURLINFO_RESPONSE_CODE,
                    &httpCode);

                std::string open;
                std::string high;
                std::string low;
                std::string close;
                std::string volume;

                std::string status;

                if (msg->data.result != CURLE_OK)
                {
                    status =
                        "HTTP_ERROR";
                }
                else if (httpCode != 200)
                {
                    status =
                        "HTTP_ERROR";
                }
                else
                {
                    open =
                        ParseField(
                            ctx->response,
                            "ราคาเปิด");

                    high =
                        ParseField(
                            ctx->response,
                            "สูงสุด");

                    low =
                        ParseField(
                            ctx->response,
                            "ต่ำสุด");

                    close =
                        ParseField(
                            ctx->response,
                            "ล่าสุด");

                    volume =
    ParseField(
        ctx->response,
        "ปริมาณ (หุ้น)");

                    if (open.empty() ||
                        high.empty() ||
                        low.empty() ||
                        close.empty() ||
                        volume.empty())
                    {
                        status =
                            "OHLCV_NOT_FOUND";
                    }
                    else
                    {
                        status =
                            "OK";
                    }
                }

                std::string marketDate =
                    ExtractMarketDate(ctx->response);

                if (!marketDate.empty())
                {
                    marketDate =
                        NormalizeMarketDate(marketDate);
                }

                if (marketDate.empty() &&
                    status == "OK")
                {
                    status = "MARKET_DATE_NOT_FOUND";
                }

                std::cout
                    << ctx->symbol
                    << "  HTTP "
                    << httpCode
                    << "  ";

                if (status == "OK")
                {
                    std::cout
                        << "O "
                        << open
                        << " H "
                        << high
                        << " L "
                        << low
                        << " C "
                        << close
                        << " V "
                        << volume;
                }
                else
                {
                    std::cout
                        << "OHLCV NOT FOUND";
                }

                std::cout
                    << "  ["
                    << status
                    << "]\n";

                if (status == "OK")
                    ++success;
                else if (status != "SKIP")
                    ++failed;

                csv
                    << ctx->symbol
                    << ","
                    << open
                    << ","
                    << high
                    << ","
                    << low
                    << ","
                    << close
                    << ","
                    << volume
                    << ","
                    << marketDate
                    << ","
                    << status
                    << "\n";

                curl_multi_remove_handle(
                    multi,
                    easy);
            }
        }

        for (auto& ctx : contexts)
        {
            if (ctx.easy)
            {
                curl_easy_cleanup(
                    ctx.easy);

                ctx.easy = nullptr;
            }
        }

        completed = batchEnd;

        std::cout
            << "Progress: "
            << completed
            << "/"
            << symbols.size()
            << "\n";
    }

    csv.close();

    int convertResult =
        ConvertPriceToAmiBroker();

    if (convertResult != 0)
    {
        curl_multi_cleanup(multi);
        curl_global_cleanup();

        return 1;
    }

    curl_multi_cleanup(
        multi);

    curl_global_cleanup();

    std::cout
        << "\n==============================\n"
        << "OHLCV CSV SAVED\n"
        << "==============================\n";

    std::cout
        << "SUCCESS : "
        << success
        << "\n";

    std::cout
        << "FAILED  : "
        << failed
        << "\n";

    std::cout
        << "TOTAL   : "
        << symbols.size()
        << "\n";

    std::cout
        << "\nCSV SAVED: set-price.csv\n";

    return 0;
}
