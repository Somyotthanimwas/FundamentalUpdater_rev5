#include "FundamentalUpdater.h"
#include "ProcessRunner.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>
#include <algorithm>
#include <map>
#include <set>
#include <windows.h>

namespace fs = std::filesystem;

// ============================================================
// FINANCIAL RECORD
// ============================================================

struct FinancialRecord
{
    std::string symbol;
    std::string quarter;
    std::string year;

    std::string totalAsset;
    std::string totalLiability;
    std::string equity;
    std::string paidupCapital;

    std::string netAssets;
    std::string netAssetsPerUnit;

    std::string totalRevenue;
    std::string totalExpense;
    std::string sales;
    std::string ebit;
    std::string ebitda;
    std::string netProfit;

    std::string profitFromOtherActivity;
    std::string eps;
    std::string netInvestmentIncome;

    std::string changeNetAssetsFromOperation;

    std::string netOperating;
    std::string netInvesting;
    std::string netFinancing;
    std::string netCashflow;

    std::string roa;
    std::string roe;
    std::string netProfitMargin;
    std::string grossProfitMargin;
    std::string deRatio;
    std::string currentRatio;
    std::string quickRatio;

    std::string dps;

    std::string investmentToTotalIncomeRatio;
    std::string debtToAssetRatio;
};


// ============================================================
// TRADING STAT RECORD
// ============================================================

struct TradingStatRecord
{
    std::string date;
    std::string period;
    std::string symbol;

    std::string market;
    std::string industry;
    std::string sector;

    std::string prior;
    std::string open;
    std::string high;
    std::string low;
    std::string average;
    std::string close;
    std::string change;
    std::string percentChange;

    std::string totalVolume;
    std::string totalValue;

    std::string pe;
    std::string pbv;
    std::string bookValuePerShare;
    std::string dividendYield;
    std::string marketCap;
    std::string listedShare;
    std::string par;

    std::string financialDate;
    std::string turnoverRatio;
    std::string beta;
    std::string dividendPayoutRatio;
    std::string averageValue;
};


// ============================================================
// JSON FIELD HELPERS
// ============================================================

static std::string ExtractString(
    const std::string& object,
    const std::string& key)
{
    std::regex pattern(
        "\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");

    std::smatch match;

    if (std::regex_search(
            object,
            match,
            pattern))
    {
        return match[1].str();
    }

    return "";
}


static std::string ExtractNumber(
    const std::string& object,
    const std::string& key)
{
    std::regex pattern(
        "\"" + key +
        "\"\\s*:\\s*(-?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[Ee][+-]?[0-9]+)?)");

    std::smatch match;

    if (std::regex_search(
            object,
            match,
            pattern))
    {
        return match[1].str();
    }

    return "";
}


// ============================================================
// FIND JSON OBJECTS
// ============================================================

static std::vector<std::string>
FindObjects(const std::string& json)
{
    std::vector<std::string> objects;

    bool inString = false;
    bool escape = false;

    int depth = 0;
    std::size_t start = std::string::npos;

    for (std::size_t i = 0;
         i < json.size();
         ++i)
    {
        char c = json[i];

        if (escape)
        {
            escape = false;
            continue;
        }

        if (c == '\\' && inString)
        {
            escape = true;
            continue;
        }

        if (c == '"')
        {
            inString = !inString;
            continue;
        }

        if (inString)
            continue;

        if (c == '{')
        {
            if (depth == 0)
                start = i;

            ++depth;
        }
        else if (c == '}')
        {
            if (depth > 0)
                --depth;

            if (depth == 0 &&
                start != std::string::npos)
            {
                objects.push_back(
                    json.substr(
                        start,
                        i - start + 1));

                start = std::string::npos;
            }
        }
    }

    return objects;
}


// ============================================================
// PARSE FINANCIAL
// ============================================================

static FinancialRecord ParseFinancialRecord(
    const std::string& object)
{
    FinancialRecord r;

    r.symbol =
        ExtractString(object, "symbol");

    r.quarter =
        ExtractString(object, "quarter");

    r.year =
        ExtractNumber(object, "year");

    r.totalAsset =
        ExtractNumber(object, "totalAsset");

    r.totalLiability =
        ExtractNumber(object, "totalLiability");

    r.equity =
        ExtractNumber(object, "equity");

    r.paidupCapital =
        ExtractNumber(object, "paidupCapital");

    r.netAssets =
        ExtractNumber(object, "netAssets");

    r.netAssetsPerUnit =
        ExtractNumber(object, "netAssetsPerUnit");

    r.totalRevenue =
        ExtractNumber(object, "totalRevenue");

    r.totalExpense =
        ExtractNumber(object, "totalExpense");

    r.sales =
        ExtractNumber(object, "sales");

    r.ebit =
        ExtractNumber(object, "ebit");

    r.ebitda =
        ExtractNumber(object, "ebitda");

    r.netProfit =
        ExtractNumber(object, "netProfit");

    r.profitFromOtherActivity =
        ExtractNumber(
            object,
            "profitFromOtherActivity");

    r.eps =
        ExtractNumber(object, "eps");

    r.netInvestmentIncome =
        ExtractNumber(
            object,
            "netInvestmentIncome");

    r.changeNetAssetsFromOperation =
        ExtractNumber(
            object,
            "changeNetAssetsFromOperation");

    r.netOperating =
        ExtractNumber(object, "netOperating");

    r.netInvesting =
        ExtractNumber(object, "netInvesting");

    r.netFinancing =
        ExtractNumber(object, "netFinancing");

    r.netCashflow =
        ExtractNumber(object, "netCashflow");

    r.roa =
        ExtractNumber(object, "roa");

    r.roe =
        ExtractNumber(object, "roe");

    r.netProfitMargin =
        ExtractNumber(object, "netProfitMargin");

    r.grossProfitMargin =
        ExtractNumber(object, "grossProfitMargin");

    r.deRatio =
        ExtractNumber(object, "deRatio");

    r.currentRatio =
        ExtractNumber(object, "currentRatio");

    r.quickRatio =
        ExtractNumber(object, "quickRatio");

    r.dps =
        ExtractNumber(object, "dps");

    if (r.dps.empty())
        r.dps =
            ExtractNumber(object, "dividendPerShare");

    if (r.dps.empty())
        r.dps =
            ExtractNumber(object, "dividendPerUnit");

    r.investmentToTotalIncomeRatio =
        ExtractNumber(
            object,
            "investmentToTotalIncomeRatio");

    r.debtToAssetRatio =
        ExtractNumber(
            object,
            "debtToAssetRatio");

    return r;
}


// ============================================================
// PARSE TRADING
// ============================================================

static TradingStatRecord ParseTradingRecord(
    const std::string& object)
{
    TradingStatRecord r;

    r.date =
        ExtractString(object, "date");

    r.period =
        ExtractString(object, "period");

    r.symbol =
        ExtractString(object, "symbol");

    r.market =
        ExtractString(object, "market");

    r.industry =
        ExtractString(object, "industry");

    r.sector =
        ExtractString(object, "sector");

    r.prior =
        ExtractNumber(object, "prior");

    r.open =
        ExtractNumber(object, "open");

    r.high =
        ExtractNumber(object, "high");

    r.low =
        ExtractNumber(object, "low");

    r.average =
        ExtractNumber(object, "average");

    r.close =
        ExtractNumber(object, "close");

    r.change =
        ExtractNumber(object, "change");

    r.percentChange =
        ExtractNumber(object, "percentChange");

    r.totalVolume =
        ExtractNumber(object, "totalVolume");

    r.totalValue =
        ExtractNumber(object, "totalValue");

    r.pe =
        ExtractNumber(object, "pe");

    r.pbv =
        ExtractNumber(object, "pbv");

    r.bookValuePerShare =
        ExtractNumber(object, "bookValuePerShare");

    r.dividendYield =
        ExtractNumber(object, "dividendYield");

    r.marketCap =
        ExtractNumber(object, "marketCap");

    r.listedShare =
        ExtractNumber(object, "listedShare");

    r.par =
        ExtractNumber(object, "par");

    r.financialDate =
        ExtractString(object, "financialDate");

    r.turnoverRatio =
        ExtractNumber(object, "turnoverRatio");

    r.beta =
        ExtractNumber(object, "beta");

    r.dividendPayoutRatio =
        ExtractNumber(
            object,
            "dividendPayoutRatio");

    r.averageValue =
        ExtractNumber(object, "averageValue");

    return r;
}


// ============================================================
// READ FILE
// ============================================================

static bool ReadFile(
    const fs::path& file,
    std::string& content)
{
    std::ifstream input(
        file,
        std::ios::binary);

    if (!input)
        return false;

    content.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());

    return true;
}


// ============================================================
// LOAD FINANCIAL JSON
// ============================================================

static bool LoadFinancialFile(
    const fs::path& file,
    std::vector<FinancialRecord>& output)
{
    std::string json;

    if (!ReadFile(file, json))
        return false;

    if (json.empty())
        return false;

    auto objects =
        FindObjects(json);

    if (objects.empty())
        return false;

    std::size_t before =
        output.size();

    for (const auto& object : objects)
    {
        FinancialRecord record =
            ParseFinancialRecord(object);

        if (!record.symbol.empty())
            output.push_back(record);
    }

    return output.size() > before;
}


// ============================================================
// LOAD TRADING JSON
// ============================================================

static bool LoadTradingFile(
    const fs::path& file,
    std::vector<TradingStatRecord>& output)
{
    std::string json;

    if (!ReadFile(file, json))
        return false;

    if (json.empty())
        return false;

    auto objects =
        FindObjects(json);

    if (objects.empty())
        return false;

    std::size_t before =
        output.size();

    for (const auto& object : objects)
    {
        TradingStatRecord record =
            ParseTradingRecord(object);

        if (!record.symbol.empty())
            output.push_back(record);
    }

    return output.size() > before;
}


// ============================================================
// CSV
// ============================================================

static void WriteFinancialCsv(
    const fs::path& file,
    const std::vector<FinancialRecord>& records)
{
    std::ofstream csv(file);

    csv
        << "Symbol,"
        << "Quarter,"
        << "Year,"
        << "TotalAsset,"
        << "TotalLiability,"
        << "Equity,"
        << "PaidupCapital,"
        << "NetAssets,"
        << "NetAssetsPerUnit,"
        << "TotalRevenue,"
        << "TotalExpense,"
        << "Sales,"
        << "EBIT,"
        << "EBITDA,"
        << "NetProfit,"
        << "ProfitFromOtherActivity,"
        << "EPS,"
        << "NetInvestmentIncome,"
        << "ChangeNetAssetsFromOperation,"
        << "NetOperating,"
        << "NetInvesting,"
        << "NetFinancing,"
        << "NetCashflow,"
        << "ROA,"
        << "ROE,"
        << "NetProfitMargin,"
        << "GrossProfitMargin,"
        << "DERatio,"
        << "CurrentRatio,"
        << "QuickRatio,"
        << "InvestmentToTotalIncomeRatio,"
        << "DebtToAssetRatio\n";

    for (const auto& r : records)
    {
        csv
            << r.symbol << ","
            << r.quarter << ","
            << r.year << ","
            << r.totalAsset << ","
            << r.totalLiability << ","
            << r.equity << ","
            << r.paidupCapital << ","
            << r.netAssets << ","
            << r.netAssetsPerUnit << ","
            << r.totalRevenue << ","
            << r.totalExpense << ","
            << r.sales << ","
            << r.ebit << ","
            << r.ebitda << ","
            << r.netProfit << ","
            << r.profitFromOtherActivity << ","
            << r.eps << ","
            << r.netInvestmentIncome << ","
            << r.changeNetAssetsFromOperation << ","
            << r.netOperating << ","
            << r.netInvesting << ","
            << r.netFinancing << ","
            << r.netCashflow << ","
            << r.roa << ","
            << r.roe << ","
            << r.netProfitMargin << ","
            << r.grossProfitMargin << ","
            << r.deRatio << ","
            << r.currentRatio << ","
            << r.quickRatio << ","
            << r.investmentToTotalIncomeRatio << ","
            << r.debtToAssetRatio
            << "\n";
    }
}


// ============================================================
// WRITE FINAL FUNDAMENTAL CSV
// 1 row per symbol
// 869 symbols
//
// Financial:
//   latest year
//
// Trading:
//   YTD if available
//   otherwise latest record
// ============================================================

static bool WriteFundamental869Csv(
    const fs::path& file,
    const fs::path& symbolsFile,
    const std::vector<FinancialRecord>& financialRecords,
    const std::vector<TradingStatRecord>& tradingRecords)
{
    // --------------------------------------------------------
    // Load 869 symbols
    // --------------------------------------------------------

    std::vector<std::string> symbols;

    {
        std::ifstream in(symbolsFile);

        if (!in)
        {
            std::cerr
                << "ERROR: Cannot open symbols.txt\n"
                << symbolsFile
                << "\n";

            return false;
        }

        std::string symbol;

        while (std::getline(in, symbol))
        {
            if (!symbol.empty())
                symbols.push_back(symbol);
        }
    }

    // --------------------------------------------------------
    // Latest Financial record per symbol
    // --------------------------------------------------------

    std::map<std::string, FinancialRecord>
        latestFinancial;

    for (const auto& r : financialRecords)
    {
        auto it = latestFinancial.find(r.symbol);

        if (it == latestFinancial.end())
        {
            latestFinancial[r.symbol] = r;
            continue;
        }

        int oldYear = 0;
        int newYear = 0;

        try
        {
            oldYear = std::stoi(it->second.year);
        }
        catch (...)
        {
        }

        try
        {
            newYear = std::stoi(r.year);
        }
        catch (...)
        {
        }

        if (newYear > oldYear)
        {
            it->second = r;
        }
        else if (newYear == oldYear &&
                 r.quarter > it->second.quarter)
        {
            it->second = r;
        }
    }

    // --------------------------------------------------------
    // Latest Trading record per symbol
    //
    // Prefer YTD.
    // Within same type, choose latest date.
    // --------------------------------------------------------

    std::map<std::string, TradingStatRecord>
        latestTrading;

    for (const auto& r : tradingRecords)
    {
        auto it = latestTrading.find(r.symbol);

        if (it == latestTrading.end())
        {
            latestTrading[r.symbol] = r;
            continue;
        }

        const bool currentYTD =
            (r.period == "YTD");

        const bool oldYTD =
            (it->second.period == "YTD");

        if (currentYTD && !oldYTD)
        {
            it->second = r;
        }
        else if (currentYTD == oldYTD)
        {
            if (r.date > it->second.date)
                it->second = r;
        }
    }

    // --------------------------------------------------------
    // Write FINAL CSV
    // --------------------------------------------------------

    std::ofstream csv(file);

    if (!csv)
    {
        std::cerr
            << "ERROR: Cannot create\n"
            << file
            << "\n";

        return false;
    }

    csv
        << "Symbol,"
        << "Last,"
        << "Chg%,"
        << "Volume,"
        << "Value(k),"
        << "MCap(M),"
        << "PE,"
        << "PBV,"
        << "DE,"
        << "DPS,"
        << "EPS,"
        << "ROA%,"
        << "ROE%,"
        << "NPM%,"
        << "Yield%,"
        << "BVPS,"
        << "SharesOut\n";

    int written = 0;
    int missingFinancial = 0;
    int missingTrading = 0;

    for (const auto& symbol : symbols)
    {
        auto fit = latestFinancial.find(symbol);
        auto tit = latestTrading.find(symbol);

        if (fit == latestFinancial.end())
        {
            ++missingFinancial;
            continue;
        }

        if (tit == latestTrading.end())
        {
            ++missingTrading;
            continue;
        }

        const auto& f = fit->second;
        const auto& t = tit->second;

        std::string valueK;
        std::string marketCapM;

        if (!t.totalValue.empty())
        {
            try
            {
                valueK =
                    std::to_string(
                        std::stod(t.totalValue) / 1000.0);
            }
            catch (...)
            {
                valueK = "";
            }
        }

        if (!t.marketCap.empty())
        {
            try
            {
                marketCapM =
                    std::to_string(
                        std::stod(t.marketCap) / 1000000.0);
            }
            catch (...)
            {
                marketCapM = "";
            }
        }

        csv
            << symbol << ","
            << t.close << ","
            << t.percentChange << ","
            << t.totalVolume << ","
            << valueK << ","
            << marketCapM << ","
            << t.pe << ","
            << t.pbv << ","
            << f.deRatio << ","
            << f.dps << ","
            << f.eps << ","
            << f.roa << ","
            << f.roe << ","
            << f.netProfitMargin << ","
            << t.dividendYield << ","
            << t.bookValuePerShare << ","
            << t.listedShare
            << "\n";

        ++written;
    }

    csv.close();

    // --------------------------------------------------------
    // Validation
    // --------------------------------------------------------

    std::cout
        << "\n==================================================\n"
        << "FINAL FUNDAMENTAL CSV\n"
        << "==================================================\n"
        << "Symbols          : "
        << symbols.size()
        << "\n"
        << "CSV rows         : "
        << written
        << "\n"
        << "Missing Financial: "
        << missingFinancial
        << "\n"
        << "Missing Trading  : "
        << missingTrading
        << "\n"
        << "Columns          : 17\n"
        << "Saved            : "
        << file
        << "\n";

    if (written != static_cast<int>(symbols.size()))
    {
        std::cerr
            << "ERROR: Fundamental CSV incomplete\n";

        return false;
    }

    if (missingFinancial != 0 ||
        missingTrading != 0)
    {
        std::cerr
            << "ERROR: Missing fundamental data\n";

        return false;
    }

    std::cout
        << "RESULT           : PASS - ALL SYMBOLS WRITTEN\n";

    return true;
}


static void WriteTradingCsv(
    const fs::path& file,
    const std::vector<TradingStatRecord>& records)
{
    std::ofstream csv(file);

    csv
        << "Date,"
        << "Period,"
        << "Symbol,"
        << "Market,"
        << "Industry,"
        << "Sector,"
        << "Prior,"
        << "Open,"
        << "High,"
        << "Low,"
        << "Average,"
        << "Close,"
        << "Change,"
        << "PercentChange,"
        << "TotalVolume,"
        << "TotalValue,"
        << "PE,"
        << "PBV,"
        << "BookValuePerShare,"
        << "DividendYield,"
        << "MarketCap,"
        << "ListedShare,"
        << "Par,"
        << "FinancialDate,"
        << "TurnoverRatio,"
        << "Beta,"
        << "DividendPayoutRatio,"
        << "AverageValue\n";

    for (const auto& r : records)
    {
        csv
            << r.date << ","
            << r.period << ","
            << r.symbol << ","
            << r.market << ","
            << r.industry << ","
            << r.sector << ","
            << r.prior << ","
            << r.open << ","
            << r.high << ","
            << r.low << ","
            << r.average << ","
            << r.close << ","
            << r.change << ","
            << r.percentChange << ","
            << r.totalVolume << ","
            << r.totalValue << ","
            << r.pe << ","
            << r.pbv << ","
            << r.bookValuePerShare << ","
            << r.dividendYield << ","
            << r.marketCap << ","
            << r.listedShare << ","
            << r.par << ","
            << r.financialDate << ","
            << r.turnoverRatio << ","
            << r.beta << ","
            << r.dividendPayoutRatio << ","
            << r.averageValue
            << "\n";
    }
}


// ============================================================
// MAIN
// ============================================================

int FundamentalUpdater::Run()
{
    std::cout
        << "==================================================\n"
        << "V4 FUNDAMENTAL UPDATE\n"
        << "==================================================\n";

    // ========================================================
    // EXE DIRECTORY
    // ========================================================

    char exeBuffer[MAX_PATH];

    DWORD exeLength =
        GetModuleFileNameA(
            nullptr,
            exeBuffer,
            MAX_PATH
        );

    if (exeLength == 0)
    {
        std::cerr
            << "ERROR: Cannot get EXE directory\n";

        return 1;
    }

    const fs::path exeDir =
        fs::path(
            std::string(exeBuffer, exeLength)
        ).parent_path();

    // ========================================================
    // OUTPUT DIRECTORY
    // ========================================================

    const fs::path outputDir =
        exeDir
        / "Data"
        / "Fundamental";

    fs::create_directories(outputDir);

    // ========================================================
    // JSON DIRECTORY
    // ========================================================

    const fs::path jsonDir =
        exeDir
        / "Data"
        / "Fundamental"
        / "JSON";

    if (!fs::exists(jsonDir))
    {
        std::cerr
            << "ERROR: JSON directory not found:\n"
            << jsonDir
            << "\n";

        return 1;
    }

    // ========================================================
    // FINANCIAL
    // ========================================================

    std::cout
        << "\n[1] FINANCIAL DATA\n"
        << "Directory : "
        << jsonDir
        << "\n";

    std::vector<FinancialRecord>
        financialRecords;

    int financialFiles = 0;
    int financialSuccess = 0;
    int financialSkip = 0;

    for (const auto& entry :
         fs::directory_iterator(jsonDir))
    {
        if (!entry.is_regular_file())
            continue;

        const std::string name =
            entry.path().filename().string();

        if (name.size() < 18)
            continue;

        if (name.find("_financial_data.json")
            == std::string::npos)
        {
            continue;
        }

        ++financialFiles;

        std::vector<FinancialRecord>
            temp;

        if (LoadFinancialFile(
                entry.path(),
                temp))
        {
            financialRecords.insert(
                financialRecords.end(),
                temp.begin(),
                temp.end());

            ++financialSuccess;

            std::cout
                << "  OK     "
                << name
                << " : "
                << temp.size()
                << " records\n";
        }
        else
        {
            ++financialSkip;

            std::cout
                << "  SKIP   "
                << name
                << "\n";
        }
    }

    std::sort(
        financialRecords.begin(),
        financialRecords.end(),
        [](const FinancialRecord& a,
           const FinancialRecord& b)
        {
            if (a.symbol != b.symbol)
                return a.symbol < b.symbol;

            if (a.year != b.year)
                return a.year < b.year;

            return a.quarter < b.quarter;
        });

    const fs::path financialCsv =
        outputDir / "fundamental.csv";

    WriteFinancialCsv(
        financialCsv,
        financialRecords);

    std::cout
        << "\nFinancial files : "
        << financialFiles
        << "\n"
        << "Financial OK    : "
        << financialSuccess
        << "\n"
        << "Financial SKIP  : "
        << financialSkip
        << "\n"
        << "Records         : "
        << financialRecords.size()
        << "\n"
        << "Saved           : "
        << financialCsv
        << "\n";


    // ========================================================
    // TRADING
    // ========================================================

    std::cout
        << "\n[2] TRADING STAT\n"
        << "Directory : "
        << jsonDir
        << "\n";

    std::vector<TradingStatRecord>
        tradingRecords;

    int tradingFiles = 0;
    int tradingSuccess = 0;
    int tradingSkip = 0;

    for (const auto& entry :
         fs::directory_iterator(jsonDir))
    {
        if (!entry.is_regular_file())
            continue;

        const std::string name =
            entry.path().filename().string();

        if (name.find("_trading_stat.json")
            == std::string::npos)
        {
            continue;
        }

        ++tradingFiles;

        std::vector<TradingStatRecord>
            temp;

        if (LoadTradingFile(
                entry.path(),
                temp))
        {
            tradingRecords.insert(
                tradingRecords.end(),
                temp.begin(),
                temp.end());

            ++tradingSuccess;

            std::cout
                << "  OK     "
                << name
                << " : "
                << temp.size()
                << " records\n";
        }
        else
        {
            ++tradingSkip;

            std::cout
                << "  SKIP   "
                << name
                << "\n";
        }
    }

    std::sort(
        tradingRecords.begin(),
        tradingRecords.end(),
        [](const TradingStatRecord& a,
           const TradingStatRecord& b)
        {
            if (a.symbol != b.symbol)
                return a.symbol < b.symbol;

            return a.period < b.period;
        });

    const fs::path tradingCsv =
        outputDir / "trading-stat.csv";

    WriteTradingCsv(
        tradingCsv,
        tradingRecords);

    std::cout
        << "\nTrading files   : "
        << tradingFiles
        << "\n"
        << "Trading OK      : "
        << tradingSuccess
        << "\n"
        << "Trading SKIP    : "
        << tradingSkip
        << "\n"
        << "Records         : "
        << tradingRecords.size()
        << "\n"
        << "Saved           : "
        << tradingCsv
        << "\n";


    // ========================================================
    // FINAL FUNDAMENTAL CSV - ONE ROW PER SYMBOL
    // ========================================================

    const fs::path symbolsFile =
        exeDir / "symbols.txt";

    const fs::path fundamental869Csv =
        outputDir / "fundamental_v4.csv";

    if (!WriteFundamental869Csv(
            fundamental869Csv,
            symbolsFile,
            financialRecords,
            tradingRecords))
    {
        return 1;
    }


    // ========================================================
    // DISPLAY SAMPLE
    // ========================================================

    std::cout
        << "\n==================================================\n"
        << "FINANCIAL SAMPLE\n"
        << "==================================================\n";

    int financialShown = 0;

    for (const auto& r : financialRecords)
    {
        std::cout
            << r.symbol
            << " "
            << r.quarter
            << " "
            << r.year
            << " "
            << "NetProfit="
            << r.netProfit
            << " EPS="
            << r.eps
            << " ROE="
            << r.roe
            << "\n";

        if (++financialShown >= 10)
            break;
    }


    std::cout
        << "\n==================================================\n"
        << "TRADING SAMPLE\n"
        << "==================================================\n";

    int tradingShown = 0;

    for (const auto& r : tradingRecords)
    {
        std::cout
            << r.symbol
            << " "
            << r.period
            << " "
            << "Close="
            << r.close
            << " PE="
            << r.pe
            << " PBV="
            << r.pbv
            << " DY="
            << r.dividendYield
            << "\n";

        if (++tradingShown >= 10)
            break;
    }


    std::cout
        << "\n==================================================\n"
        << "FUNDAMENTAL UPDATE OK\n"
        << "==================================================\n";

    return 0;
}