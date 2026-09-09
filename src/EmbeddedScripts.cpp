#include "EmbeddedScripts.h"

#include <fstream>
#include <string>

namespace EmbeddedScripts
{

bool WriteImportBat(const std::string& path)
{
    std::ofstream out(path);

    if (!out)
        return false;

    out << R"BAT(@echo off
setlocal

set "BASE=%~dp0"
set "LOG=%BASE%import_v4.log"

echo ============================================================>>"%LOG%"
echo [%date% %time%] START IMPORT>>"%LOG%"

cd /d "%BASE%"

tasklist /FI "IMAGENAME eq Broker.exe" | find /I "Broker.exe" >nul

if %errorlevel%==0 (
    echo [%date% %time%] Broker already running - hiding window>>"%LOG%"
) else (
    echo [%date% %time%] Starting Broker HIDDEN>>"%LOG%"

    powershell.exe -NoProfile -WindowStyle Hidden -Command "Start-Process -FilePath 'C:\Program Files (x86)\AmiBroker\Broker.exe' -WindowStyle Hidden"

    timeout /t 30 /nobreak >nul
)

echo [%date% %time%] Hiding Broker window>>"%LOG%"

powershell.exe -NoProfile -WindowStyle Hidden -Command "$sig='[DllImport(\"user32.dll\")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);'; Add-Type -MemberDefinition $sig -Name Win32ShowWindow -Namespace Win32; Get-Process Broker -ErrorAction SilentlyContinue | ForEach-Object { if ($_.MainWindowHandle -ne 0) { [Win32.Win32ShowWindow]::ShowWindow($_.MainWindowHandle,0) } }"

echo [%date% %time%] Running VBS>>"%LOG%"

C:\Windows\SysWOW64\cscript.exe //nologo "%BASE%import_set_v4.vbs" "%~1" >>"%LOG%" 2>&1

if %errorlevel%==0 (
    echo [%date% %time%] IMPORT SUCCESS>>"%LOG%"
) else (
    echo [%date% %time%] IMPORT FAILED ExitCode=%errorlevel%>>"%LOG%"
)

echo [%date% %time%] END IMPORT>>"%LOG%"
echo ============================================================>>"%LOG%"

endlocal
exit /b
)BAT";

    return true;
}


bool WriteImportVbs(const std::string& path)
{
    std::ofstream out(path);

    if (!out)
        return false;

    out << R"VBS(Option Explicit

Dim AB
Dim fso
Dim csvFile
Dim dbPath
Dim result

On Error Resume Next

Set fso = CreateObject("Scripting.FileSystemObject")

If WScript.Arguments.Count < 1 Then
    WScript.Echo "ERROR: CSV path argument missing"
    WScript.Quit 1
End If

csvFile = WScript.Arguments(0)

dbPath = "C:\Program Files (x86)\AmiBroker\Data"

WScript.Echo "CSV     : " & csvFile
WScript.Echo "Database: " & dbPath

If Not fso.FileExists(csvFile) Then
    WScript.Echo "ERROR: CSV not found"
    WScript.Quit 1
End If

Set AB = CreateObject("Broker.Application")

If AB Is Nothing Then
    WScript.Echo "ERROR: Cannot create Broker.Application"
    WScript.Quit 1
End If

WScript.Echo "Broker.Application created"

AB.Visible = False

result = AB.LoadDatabase(dbPath)

If result <> True Then
    WScript.Echo "ERROR: LoadDatabase failed"
    AB.Quit
    WScript.Quit 1
End If

WScript.Echo "Database loaded"

WScript.Sleep 3000

result = AB.Import(0, csvFile, "custom1.format")

WScript.Echo "Import result: " & result

WScript.Sleep 5000

AB.RefreshAll

WScript.Sleep 2000

AB.SaveDatabase

WScript.Sleep 3000

AB.Quit

Set AB = Nothing

WScript.Sleep 3000

WScript.Echo "AmiBroker import finished"

WScript.Quit 0
)VBS";

    return true;
}

}