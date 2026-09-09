#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <windows.h>
#include <map>
#include <json/json.h>

namespace fs = std::filesystem;

// ============================================================
// RECORD
// ============================================================

struct FinancialRecord
{
    std::string symbol;
    std::string year;

    std::string eps;
    std::string roa;
    std::string roe;
    std::string netProfitMargin;
    std::string deRatio;
};

struct TradingStatRecord
{
    std::string symbol;
    std::string date;

    std::string marketCap;
    std::string pe;
    std::string pbv;
    std::string dividend;
    std::string dividendYield;
};

struct HistoricalRecord
{
    std::string symbol;
    std::string date;

    std::string last;
    std::string percentChange;
    std::string volume;
    std::string value;
    std::string bookValuePerShare;
    std::string listedShare;
};

struct RelatedProductRecord
{
    std::string symbol;
    std::string last;
    std::string percentChange;
    std::string volume;
    std::string value;
};

// ============================================================
// JSON HELPERS
// ============================================================

static bool ReadJson(
    const fs::path& file,
    Json::Value& root)
{
    std::ifstream in(file);

    if (!in)
        return false;

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;

    std::string errors;

    return Json::parseFromStream(
        builder,
        in,
        &root,
        &errors);
}

static std::string JsonNumber(
    const Json::Value& object,
    const char* key)
{
    if (!object.isMember(key) ||
        object[key].isNull())
        return "";

    const Json::Value& v = object[key];

    if (v.isDouble())
        return std::to_string(v.asDouble());

    if (v.isInt() ||
        v.isUInt() ||
        v.isInt64() ||
        v.isUInt64())
        return v.asString();

    if (v.isString())
        return v.asString();

    return "";
}

static std::string JsonString(
    const Json::Value& object,
    const char* key)
{
    if (!object.isMember(key) ||
        object[key].isNull())
        return "";

    return object[key].asString();
}

// ============================================================
// FINANCIAL
// ============================================================

static bool ParseFinancial(
    const fs::path& file,
    FinancialRecord& r)
{
    Json::Value root;

    if (!ReadJson(file, root))
        return false;

    if (!root.isArray())
        return false;

    bool found = false;

    for (const auto& object : root)
    {
        FinancialRecord x;

        x.symbol =
            JsonString(object, "symbol");

        x.year =
            JsonNumber(object, "year");

        if (x.symbol.empty() ||
            x.year.empty())
            continue;

        x.eps =
            JsonNumber(object, "eps");

        x.roa =
            JsonNumber(object, "roa");

        x.roe =
            JsonNumber(object, "roe");

        x.netProfitMargin =
            JsonNumber(
                object,
                "netProfitMargin");

        x.deRatio =
            JsonNumber(
                object,
                "deRatio");

        if (!found)
        {
            r = x;
            found = true;
            continue;
        }

        int oldYear = 0;
        int newYear = 0;

        try
        {
            oldYear = std::stoi(r.year);
            newYear = std::stoi(x.year);
        }
        catch (...)
        {
        }

        if (newYear > oldYear)
            r = x;
    }

    return found;
}

// ============================================================
// TRADING STAT
// API:
// /stock/{SYMBOL}/highlight-data?lang=th
// ============================================================

static bool ParseTradingStat(
    const fs::path& file,
    TradingStatRecord& r)
{
    Json::Value root;

    if (!ReadJson(file, root))
        return false;

    if (!root.isObject())
        return false;

    r.symbol =
        JsonString(root, "symbol");

    r.date =
        JsonString(root, "asOfDate");

    if (r.symbol.empty())
        return false;

    r.marketCap =
        JsonNumber(
            root,
            "marketCap");

    r.pe =
        JsonNumber(
            root,
            "peRatio");

    r.pbv =
        JsonNumber(
            root,
            "pbRatio");

    r.dividend =
        JsonNumber(
            root,
            "dividend");

    r.dividendYield =
        JsonNumber(
            root,
            "dividendYield");

    return true;
}

// ============================================================
// HISTORICAL TRADING
// ============================================================

static bool ParseHistoricalTrading(
    const fs::path& file,
    HistoricalRecord& r)
{
    Json::Value root;

    if (!ReadJson(file, root))
        return false;

    if (!root.isArray())
        return false;

    bool found = false;

    for (const auto& object : root)
    {
        HistoricalRecord x;

        x.symbol =
            JsonString(
                object,
                "symbol");

        x.date =
            JsonString(
                object,
                "date");

        if (x.symbol.empty() ||
            x.date.empty())
            continue;

        x.last =
            JsonNumber(
                object,
                "close");

        x.percentChange =
            JsonNumber(
                object,
                "percentChange");

        x.volume =
            JsonNumber(
                object,
                "totalVolume");

        x.value =
            JsonNumber(
                object,
                "totalValue");

        x.bookValuePerShare =
            JsonNumber(
                object,
                "bookValuePerShare");

        x.listedShare =
            JsonNumber(
                object,
                "listedShare");

        if (!found ||
            x.date > r.date)
        {
            r = x;
            found = true;
        }
    }

    return found;
}

// ============================================================
// RELATED PRODUCT / CURRENT MARKET
// API:
// /stock/{SYMBOL}/related-product/o?lang=th
//
// This is the source of CURRENT market data.
// Do NOT use historical-trading for last/percentChange/volume/value.
// ============================================================

static std::string ValueK(const std::string& value)
{
    if (value.empty())
        return "";

    try
    {
        const double valueK =
            std::stod(value) / 1000.0;

        std::ostringstream out;
        out << std::fixed << std::setprecision(3)
            << valueK;

        return out.str();
    }
    catch (...)
    {
        return "";
    }
}

static bool ParseRelatedProduct(
    const fs::path& file,
    RelatedProductRecord& r)
{
    Json::Value root;

    if (!ReadJson(file, root))
        return false;

    if (!root.isObject())
        return false;

    const Json::Value& products =
        root["relatedProducts"];

    if (!products.isArray())
        return false;

    for (const auto& object : products)
    {
        if (!object.isObject())
            continue;

        const std::string securityType =
            JsonString(object, "securityType");

        const std::string symbol =
            JsonString(object, "symbol");

        // We need the ordinary stock (securityType S),
        // not futures such as AOT-F.
        if (securityType != "S")
            continue;

        if (symbol.empty())
            continue;

        r.symbol = symbol;

        r.last =
            JsonNumber(
                object,
                "last");

        r.percentChange =
            JsonNumber(
                object,
                "percentChange");

        r.volume =
            JsonNumber(
                object,
                "totalVolume");

        r.value =
            JsonNumber(
                object,
                "totalValue");

        // Current stock record found.
        return !r.last.empty();
    }

    return false;
}

// ============================================================
// MAIN
// ============================================================

int main(
    int argc,
    char* argv[])
{
    // ========================================================
    // PROJECT PATH
    //
    // FundamentalCsvConverter is a Windows/MinGW EXE
    // executed through WSL.
    //
    // Normalize the supplied WSL path explicitly instead of
    // using std::filesystem::operator/ on a mixed path.
    // ========================================================

    fs::path base;

    if (argc >= 2 && argv[1] && *argv[1])
        base = fs::path(argv[1]);
    else
        base = fs::current_path();

    std::string basePath =
        base.string();

    // Normalize separators.
    for (char& c : basePath)
    {
        if (c == '\\')
            c = '/';
    }

    while (
        basePath.size() > 1 &&
        basePath.back() == '/')
    {
        basePath.pop_back();
    }

    // ========================================================
    // WINDOWS EXE RUNNING THROUGH WSL
    //
    // argv[1] may be:
    //
    //   C:/Users/.../FundamentalUpdater_rev5
    //   /mnt/c/Users/.../FundamentalUpdater_rev5
    //   /home/thanimwas/FundamentalUpdater_rev5
    //
    // Convert WSL paths -> real Windows paths.
    // ========================================================

    if (basePath.rfind("/mnt/", 0) == 0 &&
        basePath.size() >= 7)
    {
        const char drive =
            basePath[5];

        if (drive >= 'a' && drive <= 'z')
        {
            basePath =
                std::string(1, char(drive - 'a' + 'A')) +
                ":" +
                basePath.substr(6);
        }
    }
    else if (basePath.rfind("/home/", 0) == 0)
    {
        // WSL Linux filesystem -> Windows UNC path.
        basePath =
            "\\\\wsl.localhost\\Ubuntu" +
            basePath;

        std::cerr
            << "WSL PATH -> UNC : "
            << basePath
            << "\n";
    }

    for (char& c : basePath)
    {
        if (c == '/')
            c = '\\';
    }

    const fs::path windowsBase =
        fs::path(basePath);

    const fs::path jsonDir =
        windowsBase /
        "Data" /
        "Fundamental" /
        "JSON";

    const fs::path symbolsFile =
        windowsBase /
        "symbols.txt";

    const fs::path output =
        windowsBase /
        "Data" /
        "Fundamental" /
        "fundamental_v4.csv";

    std::cout
        << "Project directory : "
        << basePath
        << "\n"
        << "Symbols file      : "
        << symbolsFile
        << "\n"
        << "JSON directory    : "
        << jsonDir
        << "\n"
        << "Output CSV        : "
        << output
        << "\n";

    std::ifstream symbolsIn(symbolsFile);

    if (!symbolsIn)
    {
        std::cerr
            << "ERROR: Cannot open symbols.txt\n";

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

    std::map<std::string, FinancialRecord>
        financial;

    std::map<std::string, TradingStatRecord>
        tradingStat;

    std::map<std::string, HistoricalRecord>
        historical;

    std::map<std::string, RelatedProductRecord>
        relatedProduct;

    int financialOK = 0;
    int tradingStatOK = 0;
    int historicalOK = 0;

    // ========================================================
    // READ ALL SYMBOLS
    // ========================================================

    int processed = 0;

    for (const auto& s : symbols)
    {
        ++processed;

        const int percent =
            static_cast<int>(
                (processed * 100.0) /
                symbols.size());

        if (processed == 1 ||
            processed % 10 == 0 ||
            processed ==
                static_cast<int>(
                    symbols.size()))
        {
            std::cout
                << "\rProcessing : "
                << processed
                << "/"
                << symbols.size()
                << " ("
                << percent
                << "%) "
                << s
                << "          "
                << std::flush;
        }

        FinancialRecord f;

        if (ParseFinancial(
                jsonDir /
                (s + "_financial_data.json"),
                f))
        {
            financial[s] = f;
            ++financialOK;
        }

        TradingStatRecord ts;

        if (ParseTradingStat(
                jsonDir /
                (s + "_trading_stat.json"),
                ts))
        {
            tradingStat[s] = ts;
            ++tradingStatOK;
        }

        HistoricalRecord h;

        if (ParseHistoricalTrading(
                jsonDir /
                (s + "_historical_trading.json"),
                h))
        {
            historical[s] = h;
            ++historicalOK;
        }

        RelatedProductRecord rp;

        if (ParseRelatedProduct(
                jsonDir /
                (s + "_related_product.json"),
                rp))
        {
            relatedProduct[s] = rp;
        }
    }

    std::cout
        << "\n\n"
        << "Financial OK   : "
        << financialOK
        << "/"
        << symbols.size()
        << "\n";

    std::cout
        << "TradingStat OK : "
        << tradingStatOK
        << "/"
        << symbols.size()
        << "\n";

    std::cout
        << "Historical OK  : "
        << historicalOK
        << "/"
        << symbols.size()
        << "\n";

    std::cout
        << "RelatedProduct : "
        << relatedProduct.size()
        << "/"
        << symbols.size()
        << "\n";

    // ========================================================
    // CREATE CSV
    //
    // IMPORTANT:
    // MinGW std::ofstream can fail when writing directly to
    // a WSL UNC path (\\wsl.localhost\...).
    //
    // Write the CSV to the Windows local TEMP directory first,
    // then copy it to the final WSL/UNC destination using
    // the Windows API.
    // ========================================================

    wchar_t tempDir[MAX_PATH] = {};

    if (GetTempPathW(MAX_PATH, tempDir) == 0)
    {
        std::cerr
            << "ERROR: Cannot get Windows TEMP path\n";

        return 1;
    }

    const fs::path tempOutput =
        fs::path(tempDir) /
        L"FundamentalCsvConverter_fundamental_v4.tmp";

    std::ofstream csv(
        tempOutput.string(),
        std::ios::out | std::ios::trunc
    );

    if (!csv)
    {
        std::cerr
            << "ERROR: Cannot create temporary CSV: "
            << tempOutput
            << "\n";

        return 1;
    }

    csv
        << "symbol,"
        << "last,"
        << "percentChange,"
        << "volume,"
        << "value,"
        << "marketCap,"
        << "pe,"
        << "pbv,"
        << "deRatio,"
        << "dps,"
        << "eps,"
        << "roa,"
        << "roe,"
        << "netProfitMargin,"
        << "dividendYield,"
        << "bookValuePerShare,"
        << "listedShare\n";

    int rows = 0;
    int skipped = 0;
    int missingFinancial = 0;
    int missingTradingStat = 0;
    int missingHistorical = 0;
    int missingRelatedProduct = 0;

    // ========================================================
    // WRITE CSV
    // ========================================================

    for (const auto& s : symbols)
    {
        auto f =
            financial.find(s);

        auto ts =
            tradingStat.find(s);

        auto h =
            historical.find(s);

        auto rp =
            relatedProduct.find(s);

        const bool hasFinancial =
            (f != financial.end());

        const bool hasTradingStat =
            (ts != tradingStat.end());

        const bool hasHistorical =
            (h != historical.end());

        const bool hasRelatedProduct =
            (rp != relatedProduct.end());

        if (!hasFinancial)
            ++missingFinancial;

        if (!hasTradingStat)
            ++missingTradingStat;

        if (!hasHistorical)
            ++missingHistorical;

        if (!hasRelatedProduct)
            ++missingRelatedProduct;

        // ----------------------------------------------------
        // SKIP SYMBOLS ONLY WHEN FUNDAMENTAL / TRADING /
        // HISTORICAL DATA IS MISSING.
        //
        // RelatedProduct is OPTIONAL because SET may return
        // securityType = "S" with last = null for symbols
        // that have no current trade.
        //
        // In that case the CSV keeps the symbol and uses
        // historical-trading data as the fallback for
        // Last / Chg% / Volume / Value.
        // ----------------------------------------------------

        if (!hasFinancial ||
            !hasTradingStat ||
            !hasHistorical)
        {
            std::cout
                << "\nSKIP "
                << s
                << " : missing";

            if (!hasFinancial)
                std::cout << " Financial";

            if (!hasTradingStat)
                std::cout << " Trading";

            if (!hasHistorical)
                std::cout << " Historical";

            std::cout << "\n";

            ++skipped;
            continue;
        }

        csv
            << s
            << ",";

        // ----------------------------------------------------
        // CURRENT MARKET DATA
        //
        // Source:
        // *_related_product.json
        //
        // DO NOT use historical-trading for these fields.
        //
        // last          -> current last price
        // percentChange -> current change %
        // totalVolume   -> current volume
        // totalValue    -> current value / 1000
        // ----------------------------------------------------

        if (hasRelatedProduct &&
            !rp->second.last.empty())
        {
            // Current market data from SET
            csv
                << rp->second.last
                << ","
                << rp->second.percentChange
                << ","
                << rp->second.volume
                << ","
                << ValueK(rp->second.value)
                << ",";
        }
        else
        {
            // Fallback to historical-trading when the SET
            // current endpoint has last = null or no product.
            csv
                << h->second.last
                << ","
                << h->second.percentChange
                << ","
                << h->second.volume
                << ","
                << ValueK(h->second.value)
                << ",";
        }

        // ----------------------------------------------------
        // Trading Stat
        // ----------------------------------------------------

        if (hasTradingStat)
        {
            csv
                << ts->second.marketCap
                << ","
                << ts->second.pe
                << ","
                << ts->second.pbv
                << ",";
        }
        else
        {
            csv
                << ",,,";
        }

        // ----------------------------------------------------
        // D/E
        // ----------------------------------------------------

        if (hasFinancial)
            csv << f->second.deRatio;

        csv << ",";

        // DPS
        // SET trading_stat.json -> dividend
        if (hasTradingStat)
            csv << ts->second.dividend;

        csv << ",";

        // ----------------------------------------------------
        // EPS / ROA / ROE / NPM
        // ----------------------------------------------------

        if (hasFinancial)
        {
            csv
                << f->second.eps
                << ","
                << f->second.roa
                << ","
                << f->second.roe
                << ","
                << f->second.netProfitMargin
                << ",";
        }
        else
        {
            csv
                << ",,,,";
        }

        // ----------------------------------------------------
        // Dividend Yield
        // ----------------------------------------------------

        if (hasTradingStat)
            csv << ts->second.dividendYield;

        csv << ",";

        // ----------------------------------------------------
        // Book Value / Listed Share
        // ----------------------------------------------------

        if (hasHistorical)
        {
            csv
                << h->second.bookValuePerShare
                << ","
                << h->second.listedShare;
        }

        csv << "\n";

        ++rows;
    }

    csv.close();

    // ========================================================
    // WRITE TEMP CSV TO FINAL OUTPUT
    //
    // Do NOT use Win32 CopyFileW here.
    // The converter runs through WSL and the final project path
    // may be exposed through the WSL filesystem layer.
    //
    // CopyFileW() can fail with Win32 Error 32 even when the
    // target file is not locked by Excel or AmiBroker.
    //
    // Read the completed TEMP CSV and write the FINAL CSV
    // directly using standard C++ streams.
    // ========================================================

    {
        std::ifstream in(
            tempOutput,
            std::ios::binary
        );

        if (!in)
        {
            std::cerr
                << "ERROR: Cannot open TEMP CSV for final write\n"
                << "TEMP : "
                << tempOutput
                << "\n";

            return 1;
        }

        std::ofstream out(
            output,
            std::ios::binary |
            std::ios::trunc
        );

        if (!out)
        {
            std::cerr
                << "ERROR: Cannot open FINAL CSV for writing\n"
                << "OUTPUT : "
                << output
                << "\n";

            return 1;
        }

        out << in.rdbuf();

        if (!out)
        {
            std::cerr
                << "ERROR: Cannot write FINAL CSV\n"
                << "OUTPUT : "
                << output
                << "\n";

            return 1;
        }

        out.close();
        in.close();
    }

    // Verify final CSV exists and is non-empty.
    if (!fs::exists(output) ||
        fs::file_size(output) == 0)
    {
        std::cerr
            << "ERROR: FINAL CSV was not created correctly\n"
            << output
            << "\n";

        return 1;
    }

    DeleteFileW(
        tempOutput.wstring().c_str()
    );

    // ========================================================
    // RESULT
    // ========================================================

    std::cout
        << "\n========================================\n"
        << "FUNDAMENTAL CSV RESULT\n"
        << "========================================\n"
        << "Rows              : "
        << rows
        << "\n"
        << "Missing Financial : "
        << missingFinancial
        << "\n"
        << "Missing Trading   : "
        << missingTradingStat
        << "\n"
        << "Missing Historical: "
        << missingHistorical
        << "\n"
        << "Skipped           : "
        << skipped
        << "\n"
        << "Output            : "
        << output
        << "\n"
        << "========================================\n";

    return 0;
}
