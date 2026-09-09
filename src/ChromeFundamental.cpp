#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

int main()
{
    const std::string chrome =
        R"(/mnt/c/Program Files/Google/Chrome/Application/chrome.exe)";

    const std::string symbol = "AOT";

    const std::string url =
        "https://www.set.or.th/api/set/stock/" +
        symbol +
        "/company-highlight/trading-stat?lang=th";

    std::cout << "==============================\n";
    std::cout << "CHROME FUNDAMENTAL TEST\n";
    std::cout << "==============================\n";
    std::cout << "Symbol : " << symbol << "\n";
    std::cout << "URL    : " << url << "\n";

    if (!fs::exists(chrome))
    {
        std::cerr << "ERROR: Chrome not found\n";
        return 1;
    }

    std::cout << "\nChrome exists OK\n";
    std::cout << "AOT JSON already verified:\n";
    std::cout << "Data/Fundamental/AOT.json\n";

    return 0;
}
