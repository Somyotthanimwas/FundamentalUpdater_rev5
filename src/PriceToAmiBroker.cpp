#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string GetField(
    std::stringstream& ss)
{
    std::string value;
    std::getline(ss, value, ',');
    return value;
}

int ConvertPriceToAmiBroker()
{
    std::ifstream input("set-price.csv");

    if (!input)
    {
        std::cerr
            << "ERROR: cannot open set-price.csv\n";
        return 1;
    }

    std::filesystem::create_directories("Data");

std::ofstream output(
    "Data/set-price-amibroker.csv");

    if (!output)
    {
        std::cerr
            << "ERROR: cannot create "
               "Data/set-price-amibroker.csv\n";
        return 1;
    }

    output
        << "<TICKER>,<DTYYYYMMDD>,<OPEN>,<HIGH>,<LOW>,<CLOSE>,<VOL>\n";

    std::string line;

    // ข้าม header
    std::getline(input, line);

    int converted = 0;
    int skipped = 0;

    while (std::getline(input, line))
    {
        if (line.empty())
            continue;

        std::stringstream ss(line);

        /*
         * set-price.csv
         *
         * Symbol,Open,High,Low,Close,Volume,Timestamp,Status
         */

        std::string symbol =
            GetField(ss);

        std::string open =
            GetField(ss);

        std::string high =
            GetField(ss);

        std::string low =
            GetField(ss);

        std::string close =
            GetField(ss);

        std::string volume =
            GetField(ss);

        std::string timestamp =
            GetField(ss);

        std::string status =
            GetField(ss);

        if (symbol.empty())
        {
            ++skipped;
            continue;
        }

        if (status != "OK")
        {
            ++skipped;
            continue;
        }

        if (open.empty() ||
            high.empty() ||
            low.empty() ||
            close.empty() ||
            volume.empty())
        {
            ++skipped;
            continue;
        }

        if (open == "-" ||
            high == "-" ||
            low == "-" ||
            close == "-" ||
            volume == "-")
        {
            ++skipped;
            continue;
        }

        if (timestamp.length() < 10)
        {
            ++skipped;
            continue;
        }

        /*
         * YYYY-MM-DD
         *
         * ->
         *
         * YYYYMMDD
         */

        std::string date =
            timestamp.substr(0, 4) +
            timestamp.substr(5, 2) +
            timestamp.substr(8, 2);

        output
            << symbol << ","
            << date << ","
            << open << ","
            << high << ","
            << low << ","
            << close << ","
            << volume << "\n";

        ++converted;
    }

    output.close();

    std::cout
        << "AmiBroker CSV created\n";

    std::cout
        << "Converted : "
        << converted
        << "\n";

    std::cout
        << "Skipped   : "
        << skipped
        << "\n";

    std::cout
        << "Output    : "
        << "Data/set-price-amibroker.csv\n";

    return 0;
}
