#include "Application.h"
#include <algorithm>
#include "PriceUpdater.h"
#include "FundamentalUpdater.h"
#include "EmbeddedFundamentalData.h"
#include "EmbeddedScripts.h"
#include "EmbeddedRunV4Excel.h"
#include "EmbeddedSetAmibroker.h"
#include "ProcessRunner.h"
#include "LineNotifier.h"
#include "Config.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <windows.h>

namespace fs = std::filesystem;

// ============================================================
// WINDOWS PATH -> WSL PATH
// ============================================================

static std::string ToWslPath(
    const fs::path& windowsPath)
{
    std::string command =
        "wsl.exe wslpath -a -u \"" +
        windowsPath.string() +
        "\"";

    FILE* pipe =
        _popen(
            command.c_str(),
            "r");

    if (!pipe)
        return {};

    char buffer[512]{};
    std::string result;

    while (fgets(
        buffer,
        sizeof(buffer),
        pipe))
    {
        result += buffer;
    }

    _pclose(pipe);

    while (
        !result.empty() &&
        (
            result.back() == '\r' ||
            result.back() == '\n'
        ))
    {
        result.pop_back();
    }

    return result;
}



// ============================================================
// GET EXE DIRECTORY
// ============================================================

static fs::path GetExeDirectory()
{
    char buffer[MAX_PATH];

    DWORD length =
        GetModuleFileNameA(
            nullptr,
            buffer,
            MAX_PATH
        );

    if (length == 0)
    {
        return fs::current_path();
    }

    return fs::path(
        std::string(buffer, length)
    ).parent_path();
}


// ============================================================
// Application::Run
// ============================================================

int Application::Run()
{
    return RunAll();
}


// ============================================================
// PRICE
// ============================================================

int Application::RunPrice()
{
    PriceUpdater updater;

    int result =
        updater.Run();

    if (result != 0)
        return result;


    // ========================================================
    // AMI BROKER IMPORT
    // ========================================================

    std::cout
        << "\n==============================\n"
        << "AMI BROKER IMPORT\n"
        << "==============================\n";


    // ========================================================
    // INSTALL / EXE DIRECTORY
    // ========================================================

    const fs::path exeDir =
        GetExeDirectory();


    // ========================================================
    // PRICE CSV
    // ========================================================

// Runtime layout:
//   Installed package:
//       package/FundamentalUpdater_rev5.exe
//       package/Data/
//
//   Development:
//       build-win/FundamentalUpdater_rev5.exe
//       project/Data/
//
// Use the directory containing Data when available.
// This keeps the package path correct on Windows.
const fs::path projectDir =
    fs::exists(exeDir / "Data")
        ? exeDir
        : exeDir.parent_path();

const fs::path csvFile =
    projectDir /
    "Data" /
    "set-price-amibroker.csv";



    if (!fs::exists(csvFile))
    {
        std::cerr
            << "ERROR: AmiBroker CSV not found:\n"
            << csvFile
            << "\n";

        return 1;
    }


    std::cout
        << "EXE DIR : "
        << exeDir
        << "\n"
        << "CSV     : "
        << csvFile
        << "\n";


    // ========================================================
    // WINDOWS TEMP CSV BRIDGE
    //
    // AmiBroker cannot access WSL UNC paths reliably.
    // Copy CSV through wsl.exe -> Windows TEMP.
    //
    // DO NOT USE C:\\Scripts
    // ========================================================

    const char* tempEnv =
        std::getenv("TEMP");

    if (tempEnv == nullptr)
    {
        std::cerr
            << "ERROR: Windows TEMP is not available\n";

        return 1;
    }

    const fs::path tempDir =
        fs::path(tempEnv) /
        "FundamentalUpdater_rev5";

    fs::create_directories(
        tempDir
    );

    const fs::path tempCsv =
        tempDir /
        "set-price-amibroker.csv";

    std::cout
        << "Windows TEMP CSV : "
        << tempCsv
        << "\n";

    // Convert the CSV path to a path usable inside WSL.
    //
    // When the Windows EXE is launched from WSL, the path may be:
    //
    //   \\wsl.localhost\Ubuntu\home\thanimwas\...
    //
    // In that case do NOT call wslpath on it. Convert the WSL UNC
    // path directly back to its Linux path.
    //
    // When running from a normal Windows path such as:
    //
    //   C:\...\Data\set-price-amibroker.csv
    //
    // use wslpath to convert it.

    std::string csvPathForWsl =
        csvFile.string();

    const std::string wslPrefix1 =
        "\\\\wsl.localhost\\Ubuntu\\";

    const std::string wslPrefix2 =
        "\\\\wsl$\\Ubuntu\\";

    if (csvPathForWsl.rfind(wslPrefix1, 0) == 0)
    {
        csvPathForWsl =
            "/" +
            csvPathForWsl.substr(wslPrefix1.size());

        std::replace(
            csvPathForWsl.begin(),
            csvPathForWsl.end(),
            '\\',
            '/'
        );
    }
    else if (csvPathForWsl.rfind(wslPrefix2, 0) == 0)
    {
        csvPathForWsl =
            "/" +
            csvPathForWsl.substr(wslPrefix2.size());

        std::replace(
            csvPathForWsl.begin(),
            csvPathForWsl.end(),
            '\\',
            '/'
        );
    }
    else
    {
        // Normal Windows path: let WSL convert it.
        const std::string windowsPath =
            csvPathForWsl;

        csvPathForWsl =
            "$(wslpath -a '" +
            windowsPath +
            "')";
    }

    std::cout
        << "WSL source CSV : "
        << csvPathForWsl
        << "\n";

    const std::string copyCommand =
        "cmd.exe /d /c \"cd /d C:\\Windows && "
        "wsl.exe -d Ubuntu -- cat \"" +
        csvPathForWsl +
        "\" > \"" +
        tempCsv.string() +
        "\"";

    std::cout
        << "Copying CSV to Windows TEMP...\n";

    ProcessRunner copyRunner;

    if (!copyRunner.Run(copyCommand))
    {
        std::cerr
            << "ERROR: Cannot copy CSV to Windows TEMP\n";

        return 1;
    }

    if (!fs::exists(tempCsv))
    {
        std::cerr
            << "ERROR: Windows TEMP CSV was not created:\n"
            << tempCsv
            << "\n";

        return 1;
    }

    std::cout
        << "Windows TEMP CSV ready.\n";


    // ========================================================
    // CREATE EMBEDDED IMPORT SCRIPTS
    // ========================================================

    const fs::path importBatPath =
        exeDir /
        "import_set_v4.bat";

    const fs::path importVbsPath =
        exeDir /
        "import_set_v4.vbs";


    std::cout
        << "Creating import scripts...\n";


    if (!EmbeddedScripts::WriteImportBat(
            importBatPath.string()))
    {
        std::cerr
            << "ERROR: Cannot create:\n"
            << importBatPath
            << "\n";

        return 1;
    }


    if (!EmbeddedScripts::WriteImportVbs(
            importVbsPath.string()))
    {
        std::cerr
            << "ERROR: Cannot create:\n"
            << importVbsPath
            << "\n";

        return 1;
    }


    std::cout
        << "Import BAT : "
        << importBatPath
        << "\n"
        << "Import VBS : "
        << importVbsPath
        << "\n";


    // ========================================================
    // CHECK IMPORT SCRIPT
    // ========================================================

    if (!fs::exists(importBatPath) ||
        !fs::exists(importVbsPath))
    {
        std::cerr
            << "ERROR: Import scripts were not created.\n";

        return 1;
    }


    // ========================================================
    // AMIBROKER IMPORT
    // ========================================================

    const std::string windowsCsv =
        tempCsv.string();

    const std::string importBat =
        importBatPath.string();

    const std::string importCommand =
        "cmd.exe /c call \""
        + importBat
        + "\" \""
        + windowsCsv
        + "\"";


    std::cout
        << "Running AmiBroker import...\n";


    ProcessRunner runner;


    if (!runner.Run(importCommand))
    {
        std::cerr
            << "ERROR: AmiBroker Import FAILED\n";

        return 1;
    }


    std::cout
        << "AmiBroker Import OK\n"
        << "Set import AmiBroker สำเร็จ\n";


    // ========================================================
    // CLEAN WINDOWS TEMP CSV
    // ========================================================

    std::error_code removeError;

    fs::remove(
        tempCsv,
        removeError
    );

    if (removeError)
    {
        std::cerr
            << "WARNING: Cannot remove TEMP CSV: "
            << tempCsv
            << "\n";
    }
    else
    {
        std::cout
            << "TEMP CSV removed.\n";
    }


    // ========================================================
    // LINE notification - PRICE ONLY
    // ========================================================

    Config config;


    if (!config.Load(
        (projectDir / "config.ini").string()
    ))
    {
        std::cerr
            << "LINE config load FAILED\n";

        return 1;
    }


    LineNotifier line;


    bool ok =
        line.Send(
            config.Get("AccessToken"),
            config.Get("UserId"),
            "Set import AmiBroker สำเร็จ"
        );


    if (ok)
    {
        std::cout
            << "LINE Send OK\n";
    }
    else
    {
        std::cerr
            << "LINE Send FAILED\n";
    }


    return 0;
}


// ============================================================
// FUNDAMENTAL
// ============================================================

int Application::RunFundamental()
{
    std::cout
        << "\n==================================================\n"
        << "V4 FUNDAMENTAL UPDATE\n"
        << "==================================================\n";

    // ========================================================
    // PROJECT DIRECTORY
    // ========================================================

    const fs::path exeDir =
        GetExeDirectory();

    // Runtime layout:
    //
    // 1) Development:
    //      project/build-win/*.exe
    //      project/Data/
    //
    // 2) Installed:
    //      package/*.exe
    //      package/Data/
    //
    // Detect the layout instead of blindly using parent_path().
    const fs::path projectDir =
        fs::exists(exeDir / "Data")
            ? exeDir
            : exeDir.parent_path();

    std::cout
        << "Project : "
        << projectDir
        << "\n";


    // ========================================================
    // FUNDAMENTAL CAPTURE
    //
    // Generate fresh SET JSON files.
    // ========================================================

    const fs::path captureExe =
        exeDir /
        "FundamentalCapture.exe";

    std::cout
        << "\n========================================\n"
        << "FUNDAMENTAL JSON CAPTURE\n"
        << "========================================\n";

    // ========================================================
    // WSL PATH CONVERSION
    //
    // When the application is launched from WSL, projectDir and
    // captureExe are WSL UNC paths:
    //
    //   \\wsl.localhost\Ubuntu\home\thanimwas\...
    //
    // Convert them directly to:
    //
    //   /home/thanimwas/...
    //
    // NEVER pass a WSL UNC path to wslpath.
    // ========================================================

    auto WslUncToLinux =
        [](const fs::path& p) -> std::string
    {
        std::string s = p.string();

        const std::string prefix1 =
            "\\\\wsl.localhost\\Ubuntu\\";

        const std::string prefix2 =
            "\\\\wsl$\\Ubuntu\\";

        if (s.rfind(prefix1, 0) == 0)
        {
            std::string result =
                "/" + s.substr(prefix1.size());

            std::replace(
                result.begin(),
                result.end(),
                '\\',
                '/');

            return result;
        }

        if (s.rfind(prefix2, 0) == 0)
        {
            std::string result =
                "/" + s.substr(prefix2.size());

            std::replace(
                result.begin(),
                result.end(),
                '\\',
                '/');

            return result;
        }

        // When the Windows EXE is launched from WSL,
        // std::filesystem may expose a WSL UNC path as:
        //
        //   /mnt/c/wsl.localhost/Ubuntu/home/...
        //
        // This is NOT a real Linux path.
        // Strip the Windows mount prefix and recover:
        //
        //   /home/...

        const std::string mountedWslPrefix =
            "/mnt/c/wsl.localhost/Ubuntu";

        if (s.rfind(mountedWslPrefix, 0) == 0)
        {
            std::string result =
                s.substr(mountedWslPrefix.size());

            if (result.empty() ||
                result[0] != '/')
            {
                result = "/" + result;
            }

            std::replace(
                result.begin(),
                result.end(),
                '\\',
                '/');

            return result;
        }

        return ToWslPath(p);
    };

    const std::string wslProjectDir =
        WslUncToLinux(projectDir);

    const std::string wslCaptureExe =
        WslUncToLinux(captureExe);

    if (wslProjectDir.empty() ||
        wslCaptureExe.empty())
    {
        std::cerr
            << "ERROR: Cannot convert Fundamental paths to WSL paths.\n"
            << "Project : "
            << projectDir
            << "\n"
            << "Capture : "
            << captureExe
            << "\n";

        return 1;
    }

    // ========================================================
    // VERIFY FUNDAMENTAL CAPTURE IN WSL
    //
    // Do NOT use std::filesystem::exists() here.
    //
    // The Windows EXE may see the Linux file as a WSL UNC path,
    // while FundamentalCapture.exe actually runs inside Ubuntu.
    //
    // Therefore verify the file from inside WSL.
    // ========================================================

    {
        const std::string verifyCommand =
            "wsl.exe -d Ubuntu test -f \"" +
            wslCaptureExe +
            "\"";

        const int verifyResult =
            std::system(verifyCommand.c_str());

        if (verifyResult != 0)
        {
            std::cerr
                << "ERROR: FundamentalCapture.exe not found inside WSL.\n"
                << "Windows path : "
                << captureExe
                << "\n"
                << "WSL path     : "
                << wslCaptureExe
                << "\n";

            return 1;
        }
    }

    std::cout
        << "Capture EXE verified : "
        << wslCaptureExe
        << "\n";

    // ========================================================
    // PREPARE WSL PATHS FOR FUNDAMENTAL CAPTURE
    // ========================================================

    // FundamentalCapture.exe is launched through WSL.
    // Both the executable and project argument must therefore
    // use real Linux/WSL paths.

    // When this application itself is launched through WSL,
    // GetExeDirectory() can return a WSL UNC path.
    //
    // Convert:
    //   /mnt/c/wsl.localhost/Ubuntu/home/...
    //
    // back to the real WSL path:
    //   /home/...

    std::string captureCdPath = wslProjectDir;
    std::string captureExePath = wslCaptureExe;

    const std::string badPrefix =
        "/mnt/c/wsl.localhost/Ubuntu";

    if (captureCdPath.rfind(badPrefix, 0) == 0)
    {
        captureCdPath =
            captureCdPath.substr(badPrefix.size());

        if (captureCdPath.empty() ||
            captureCdPath[0] != '/')
        {
            captureCdPath = "/" + captureCdPath;
        }
    }

    if (captureExePath.rfind(badPrefix, 0) == 0)
    {
        captureExePath =
            captureExePath.substr(badPrefix.size());

        if (captureExePath.empty() ||
            captureExePath[0] != '/')
        {
            captureExePath = "/" + captureExePath;
        }
    }

    std::cout
        << "WSL capture project : "
        << captureCdPath
        << "\n"
        << "WSL capture EXE     : "
        << captureExePath
        << "\n";

    // FundamentalCapture.exe runs INSIDE WSL.
    // Therefore argv[1] must be the real Linux project path,
    // NOT the Windows WSL-UNC path.
    const std::string captureProjectArgument =
        captureCdPath;

    const std::string captureCommand =
        "wsl.exe -d Ubuntu "
        "--cd \"" + captureCdPath + "\" "
        "\"" + captureExePath + "\" "
        "\"" + captureProjectArgument + "\"";

    ProcessRunner captureRunner;

    if (!captureRunner.Run(captureCommand))
    {
        std::cerr
            << "ERROR: Fundamental JSON capture failed.\n";

        return 1;
    }


    // ========================================================
    // VERIFY JSON
    // ========================================================

    const fs::path jsonDir =
        projectDir /
        "Data" /
        "Fundamental" /
        "JSON";

    if (!fs::exists(jsonDir))
    {
        std::cerr
            << "ERROR: Fundamental JSON directory not found:\n"
            << jsonDir
            << "\n";

        return 1;
    }

    std::size_t jsonCount = 0;

    for (const auto& entry :
         fs::directory_iterator(jsonDir))
    {
        if (!entry.is_regular_file())
            continue;

        const auto name =
            entry.path().filename().string();

        if (name.size() > 5 &&
            name.substr(name.size() - 5) == ".json")
        {
            ++jsonCount;
        }
    }

    std::cout
        << "JSON files : "
        << jsonCount
        << "\n";

    if (jsonCount == 0)
    {
        std::cerr
            << "ERROR: No Fundamental JSON files captured.\n";

        return 1;
    }


    // ========================================================
    // CSV CONVERTER
    //
    // JSON -> fundamental_v4.csv
    // ========================================================

    const fs::path converterExe =
        exeDir /
        "FundamentalCsvConverter.exe";

    if (!fs::exists(converterExe))
    {
        std::cerr
            << "ERROR: FundamentalCsvConverter.exe not found:\n"
            << converterExe
            << "\n";

        return 1;
    }

    std::cout
        << "\n========================================\n"
        << "FUNDAMENTAL JSON -> CSV\n"
        << "========================================\n";

    const std::string wslConverterExe =
        WslUncToLinux(converterExe);

    if (wslConverterExe.empty())
    {
        std::cerr
            << "ERROR: Cannot convert Converter path to WSL path.\n";

        return 1;
    }

    // ========================================================
    // CONVERTER PATH
    //
    // WslUncToLinux() has already converted the Windows/WSL
    // paths to real Linux paths.
    //
    // Do NOT apply badPrefix conversion again here.
    // ========================================================

    const std::string converterCdPath =
        wslProjectDir;

    const std::string converterExePath =
        wslConverterExe;

    std::cout
        << "WSL converter project : "
        << converterCdPath
        << "\n"
        << "WSL converter EXE     : "
        << converterExePath
        << "\n";

    // FundamentalCsvConverter.exe also runs INSIDE WSL.
    // Pass the real Linux project path directly.
    const std::string converterProjectArgument =
        converterCdPath;

    const std::string converterCommand =
        "wsl.exe -d Ubuntu "
        "--cd \"" + converterCdPath + "\" "
        "\"" + converterExePath + "\" "
        "\"" + converterProjectArgument + "\"";

    ProcessRunner converterRunner;

    if (!converterRunner.Run(converterCommand))
    {
        std::cerr
            << "ERROR: Fundamental CSV conversion failed.\n";

        return 1;
    }


    // ========================================================
    // VERIFY fundamental_v4.csv
    // ========================================================

    const fs::path sourceCsv =
        projectDir /
        "Data" /
        "Fundamental" /
        "fundamental_v4.csv";

    if (!fs::exists(sourceCsv))
    {
        std::cerr
            << "ERROR: fundamental_v4.csv not found:\n"
            << sourceCsv
            << "\n";

        return 1;
    }

    const auto sourceSize =
        fs::file_size(sourceCsv);

    if (sourceSize == 0)
    {
        std::cerr
            << "ERROR: fundamental_v4.csv is empty.\n";

        return 1;
    }

    std::cout
        << "CSV source : "
        << sourceCsv
        << "\n"
        << "CSV size   : "
        << sourceSize
        << " bytes\n";


    // ========================================================
    // fundamental_v4.csv is already the FINAL V4 CSV
    //
    // This is the CSV consumed by Macro 2.
    // No copy/remove step is required.
    // ========================================================

    // ========================================================
    // VERIFY COLUMN COUNT
    // ========================================================

    std::ifstream verify(
        sourceCsv
    );

    if (!verify)
    {
        std::cerr
            << "ERROR: Cannot open final fundamental_v4.csv\n";

        return 1;
    }

    std::string header;

    if (!std::getline(
            verify,
            header))
    {
        std::cerr
            << "ERROR: Cannot read final CSV header.\n";

        return 1;
    }

    int columns = 1;

    for (char c : header)
    {
        if (c == ',')
            ++columns;
    }

    std::cout
        << "Final columns : "
        << columns
        << "\n";

    if (columns != 17)
    {
        std::cerr
            << "ERROR: Unexpected Fundamental CSV columns.\n"
            << "Expected : 17\n"
            << "Actual   : "
            << columns
            << "\n";

        return 1;
    }


    std::cout
        << "\n========================================\n"
        << "FUNDAMENTAL V4 UPDATE : SUCCESS\n"
        << "========================================\n"
        << "JSON files : "
        << jsonCount
        << "\n"
        << "CSV rows   : generated from JSON\n"
        << "CSV        : "
        << sourceCsv
        << "\n"
        << "Columns    : "
        << columns
        << "\n"
        << "========================================\n";

    return 0;
}


// ============================================================
// RUN ALL
// ============================================================

int Application::RunAll()
{
    std::cout
        << "\n==================================================\n"
        << "V5 ALL UPDATE\n"
        << "==================================================\n";


    // ========================================================
    // SET WORKING DIRECTORY TO PROJECT ROOT
    //
    // PriceUpdater V4 uses:
    //     symbols.txt
    //
    // symbols.txt is located in:
    //     FundamentalUpdater_rev5/symbols.txt
    //
    // The EXE is normally launched from:
    //     FundamentalUpdater_rev5/build-win/
    //
    // Therefore make the project root the working directory
    // before PriceUpdater::Run() is called.
    // ========================================================

    const fs::path runExeDir =
        GetExeDirectory();

    fs::path runProjectDir =
        runExeDir;

    if (!fs::exists(
        runProjectDir / "symbols.txt"))
    {
        runProjectDir =
            runExeDir.parent_path();
    }

    if (!fs::exists(
        runProjectDir / "symbols.txt"))
    {
        std::cerr
            << "ERROR: symbols.txt not found:\n"
            << "EXE DIR     : "
            << runExeDir
            << "\n"
            << "PROJECT DIR : "
            << runProjectDir
            << "\n";

        return 1;
    }

    std::error_code cwdError;

    fs::current_path(
        runProjectDir,
        cwdError);

    if (cwdError)
    {
        std::cerr
            << "ERROR: Cannot set working directory:\n"
            << runProjectDir
            << "\n"
            << cwdError.message()
            << "\n";

        return 1;
    }

    std::cout
        << "PROJECT DIR : "
        << runProjectDir
        << "\n"
        << "WORKING DIR : "
        << fs::current_path()
        << "\n"
        << "SYMBOLS     : "
        << (runProjectDir / "symbols.txt")
        << "\n";


    // ========================================================
// 1. PRICE V4
// ========================================================

std::cout
    << "\n==================================================\n"
    << "STEP 1 : PRICE V4\n"
    << "==================================================\n";

int result =
    RunPrice();

if (result != 0)
{
    std::cerr
        << "ERROR: PRICE V4 FAILED\n";

    return result;
}

std::cout
    << "STEP 1 : PRICE V4 SUCCESS\n";


// ========================================================
// 2. FUNDAMENTAL V4
// ========================================================

std::cout
    << "\n==================================================\n"
    << "STEP 2 : FUNDAMENTAL V4\n"
    << "==================================================\n";

result =
    RunFundamental();

if (result != 0)
{
    std::cerr
        << "ERROR: FUNDAMENTAL V4 FAILED\n";

    return result;
}

std::cout
    << "STEP 2 : FUNDAMENTAL V4 SUCCESS\n";


    // ========================================================
    // FINAL FUNDAMENTAL V4 CSV
    //
    // fundamental_v4.csv is generated by Fundamental V4 /
    // FundamentalCsvConverter at runtime.
    //
    // IMPORTANT:
    // DO NOT restore embedded CSV here.
    // The embedded CSV may be stale and would overwrite the
    // freshly generated runtime CSV.
    // ========================================================

    {
        const fs::path finalCsv =
            runProjectDir /
            "Data" /
            "Fundamental" /
            "fundamental_v4.csv";

        if (!fs::exists(finalCsv) ||
            fs::file_size(finalCsv) == 0)
        {
            std::cerr
                << "ERROR: Final fundamental_v4.csv is missing or empty:\n"
                << finalCsv
                << "\n";

            return 1;
        }

        std::cout
            << "FINAL FUNDAMENTAL CSV : READY\n"
            << "Path  : "
            << finalCsv
            << "\n"
            << "Bytes : "
            << fs::file_size(finalCsv)
            << "\n";
    }


    // ========================================================
    // EXCEL V4
    //
    // Embedded:
    //   1. run_v4_excel.ps1
    //   2. Set_Amibroker_ok.xlsm
    //
    // User does not need either file separately.
    // fundamental_v4.csv is generated by V4 at runtime.
    // ========================================================

    const fs::path exeDir =
        GetExeDirectory();

    fs::path projectDir =
        exeDir;

    // EXE normally resides in:
    //     project/build/FundamentalUpdater_rev5.exe
    //
    // Runtime data is kept in:
    //     project/Data/

    if (!fs::exists(
        projectDir / "Data"
    ))
    {
        projectDir =
            exeDir.parent_path();
    }

    const fs::path dataDir =
        projectDir / "Data";

    const fs::path excelScript =
        projectDir /
        ".run_v4_excel_embedded.ps1";

    const fs::path workbook =
        dataDir /
        "Set_Amibroker_ok.xlsm";


    std::cout
        << "\n==================================================\n"
        << "EXCEL V4\n"
        << "==================================================\n"
        << "Embedded Excel workbook + PowerShell\n";


    // ========================================================
    // CREATE TEMPORARY XLSM FROM EMBEDDED WORKBOOK
    // ========================================================

    {
        std::ofstream out(
            workbook,
            std::ios::binary
        );

        if (!out)
        {
            std::cerr
                << "ERROR: Cannot create embedded workbook:\n"
                << workbook
                << "\n";

            return 1;
        }

        out.write(
            reinterpret_cast<const char*>(
                Data_Set_Amibroker_ok_xlsm
            ),
            Data_Set_Amibroker_ok_xlsm_len
        );

        if (!out)
        {
            std::cerr
                << "ERROR: Cannot write embedded workbook\n";

            return 1;
        }
    }


    // ========================================================
    // CREATE TEMPORARY PS1 FROM EMBEDDED SCRIPT
    // ========================================================

    {
        std::ofstream out(
            excelScript,
            std::ios::binary
        );

        if (!out)
        {
            std::cerr
                << "ERROR: Cannot create embedded Excel script:\n"
                << excelScript
                << "\n";

            return 1;
        }

        out.write(
            reinterpret_cast<const char*>(
                run_v4_excel_ps1
            ),
            run_v4_excel_ps1_len
        );

        if (!out)
        {
            std::cerr
                << "ERROR: Cannot write embedded Excel script\n";

            return 1;
        }
    }


    // ========================================================
    // RUN EMBEDDED EXCEL WORKFLOW
    // ========================================================

    const std::string powershellCommand =
        "powershell.exe "
        "-NoProfile "
        "-ExecutionPolicy Bypass "
        "-File "
        "\""
        + excelScript.string()
        + "\"";


    std::cout
        << "Running embedded Excel V4...\n";


    ProcessRunner excelRunner;


    if (!excelRunner.Run(
        powershellCommand
    ))
    {
        std::cerr
            << "ERROR: Excel V4 FAILED\n";

        try
        {
            fs::remove(excelScript);
        }
        catch (...) {}

        // Keep workbook for diagnostic purposes on failure.
        return 1;
    }


    std::cout
        << "Excel V4 completed successfully.\n";


    // ========================================================
    // REMOVE TEMPORARY EMBEDDED FILES
    // ========================================================

    try
    {
        fs::remove(excelScript);
    }
    catch (...) {}

    try
    {
        fs::remove(workbook);
    }
    catch (...) {}

    // ========================================================
    // LINE notification - V5 completed
    // ========================================================

    Config config;


    if (!config.Load(
        (projectDir / "config.ini").string()
    ))
    {
        std::cerr
            << "LINE config load FAILED\n";

        return 1;
    }


    LineNotifier line;


    bool ok =
        line.Send(
            config.Get("AccessToken"),
            config.Get("UserId"),
            "V5 Update OK - Price + Fundamental + Excel Macro + AmiBroker เสร็จเรียบร้อย"
        );


    if (ok)
    {
        std::cout
            << "LINE Send OK\n";
    }
    else
    {
        std::cerr
            << "LINE Send FAILED\n";
    }


    return 0;
}
