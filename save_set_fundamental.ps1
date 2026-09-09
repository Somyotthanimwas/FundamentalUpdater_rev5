param(
    [string]$Symbol
)

$chrome = "C:\Program Files\Google\Chrome\Application\chrome.exe"
$outDir = "C:\Scripts\Fundamental"
$url = "https://www.set.or.th/api/set/stock/$Symbol/company-highlight/trading-stat?lang=th"
$outFile = Join-Path $outDir "$Symbol.json"

Write-Host "=============================="
Write-Host "SET FUNDAMENTAL CHROME"
Write-Host "=============================="
Write-Host "Symbol : $Symbol"
Write-Host "URL    : $url"
Write-Host "Output : $outFile"

if (!(Test-Path $chrome)) {
    Write-Host "ERROR: Chrome not found"
    exit 1
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

if (Test-Path $outFile) {
    Remove-Item $outFile -Force
}

# ------------------------------------------------------------
# Chrome profile ชั่วคราว
# ------------------------------------------------------------

$profileDir = Join-Path $env:TEMP "SETFundamental_$Symbol"

if (Test-Path $profileDir) {
    Remove-Item $profileDir -Recurse -Force -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force -Path $profileDir | Out-Null

Write-Host "Chrome Profile : $profileDir"

# ------------------------------------------------------------
# Win32
# ------------------------------------------------------------

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public class Win32 {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(
        EnumWindowsProc lpEnumFunc,
        IntPtr lParam
    );

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern int GetWindowText(
        IntPtr hWnd,
        StringBuilder lpString,
        int nMaxCount
    );

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(
        IntPtr hWnd,
        int nCmdShow
    );

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(
        IntPtr hWnd
    );
}
"@

# ------------------------------------------------------------
# เปิด Chrome
# ------------------------------------------------------------

Write-Host "Starting Chrome..."

Start-Process `
    -FilePath $chrome `
    -ArgumentList @(
        "--user-data-dir=$profileDir",
        "--new-window",
        "--no-first-run",
        "--no-default-browser-check",
        $url
    )

Write-Host "Chrome started"

# ------------------------------------------------------------
# หา SET Chrome Window
# ------------------------------------------------------------

$windowHandle = [IntPtr]::Zero
$windowTitle = ""

for ($attempt = 1; $attempt -le 40; $attempt++) {

    $foundHandle = [IntPtr]::Zero
    $foundTitle = ""

    $callback = [Win32+EnumWindowsProc] {
        param($hWnd, $lParam)

        if (![Win32]::IsWindowVisible($hWnd)) {
            return $true
        }

        $sb = New-Object System.Text.StringBuilder 2048

        [Win32]::GetWindowText(
            $hWnd,
            $sb,
            $sb.Capacity
        ) | Out-Null

        $title = $sb.ToString()

        if ($title -and
            $title -match "set\.or\.th" -and
            $title -match [regex]::Escape($Symbol)) {

            $script:foundHandle = $hWnd
            $script:foundTitle = $title

            return $false
        }

        return $true
    }

    [Win32]::EnumWindows(
        $callback,
        [IntPtr]::Zero
    ) | Out-Null

    if ($foundHandle -ne [IntPtr]::Zero) {
        $windowHandle = $foundHandle
        $windowTitle = $foundTitle
        break
    }

    Start-Sleep -Milliseconds 500
}

if ($windowHandle -eq [IntPtr]::Zero) {
    Write-Host "ERROR: Cannot find SET Chrome window"
    exit 1
}

Write-Host ""
Write-Host "SET Chrome Window Found"
Write-Host "Window Handle : $windowHandle"
Write-Host "Window Title  : $windowTitle"

# ------------------------------------------------------------
# Activate
# ------------------------------------------------------------

[Win32]::ShowWindow($windowHandle, 9) | Out-Null
[Win32]::SetForegroundWindow($windowHandle) | Out-Null

Start-Sleep -Seconds 2

Write-Host "Chrome window activated"

# ------------------------------------------------------------
# WScript Shell
# ------------------------------------------------------------

$shell = New-Object -ComObject WScript.Shell

if ($null -eq $shell) {
    Write-Host "ERROR: Cannot create WScript.Shell"
    exit 1
}

# ------------------------------------------------------------
# Save As
# ------------------------------------------------------------

Write-Host "Sending Ctrl+S..."

$shell.SendKeys("^s")

Start-Sleep -Seconds 3

Write-Host "Save dialog opened"

# ------------------------------------------------------------
# กำหนดชื่อไฟล์และ Save อัตโนมัติ
# ------------------------------------------------------------

Write-Host "Setting output filename..."

Start-Sleep -Seconds 1

# File name ช่องของ Windows Save Dialog
$shell.SendKeys("^a")

Start-Sleep -Milliseconds 300

$shell.SendKeys($outFile)

Start-Sleep -Milliseconds 500

Write-Host "Saving: $outFile"

$shell.SendKeys("{ENTER}")

# ------------------------------------------------------------
# รอให้บันทึก
# ------------------------------------------------------------

Start-Sleep -Seconds 4

# ------------------------------------------------------------
# ตรวจสอบไฟล์
# ------------------------------------------------------------

if (Test-Path $outFile) {

    $size = (Get-Item $outFile).Length

    Write-Host ""
    Write-Host "================================"
    Write-Host "SAVE SUCCESS"
    Write-Host "================================"
    Write-Host "File : $outFile"
    Write-Host "Size : $size bytes"

}
else {

    Write-Host ""
    Write-Host "================================"
    Write-Host "SAVE FAILED"
    Write-Host "================================"
    Write-Host "File not found:"
    Write-Host $outFile

    exit 1
}

exit 0
