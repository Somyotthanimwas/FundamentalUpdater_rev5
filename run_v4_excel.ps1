$ErrorActionPreference = "Stop"

Write-Host "========================================"
Write-Host "V4 EXCEL MACRO RUNNER"
Write-Host "========================================"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$workbook = Join-Path $scriptDir "Data\Set_Amibroker_ok.xlsm"

Write-Host "Workbook:"
Write-Host $workbook

if (-not (Test-Path $workbook)) {
    Write-Host "ERROR: Workbook not found"
    exit 1
}

$excel = $null
$wb = $null

try {

    # ========================================================
    # START EXCEL
    # ========================================================

    Write-Host "Starting Excel..."

    $excel = New-Object -ComObject Excel.Application

    $excel.Visible = $false
    $excel.DisplayAlerts = $false


    # ========================================================
    # OPEN WORKBOOK
    # ========================================================

    Write-Host "Opening workbook..."

    $wb = $excel.Workbooks.Open($workbook)

    Start-Sleep -Seconds 2


    # ========================================================
    # MACRO 2
    # Fundamental CSV -> Excel
    # ========================================================

    Write-Host "Running Macro 2 V4..."

    $excel.Run("UpdateV4All17")

    Start-Sleep -Seconds 2

    Write-Host "Macro 2 completed."


    # ========================================================
    # SAVE AFTER MACRO 2
    # ========================================================

    Write-Host "Saving workbook after Macro 2..."

    $wb.Save()

    Start-Sleep -Seconds 2

    Write-Host "Macro 2 SAVE completed."


        # ========================================================
    # PREPARE FOR MACRO 1
    # ========================================================

    Write-Host "Activating Stocks_Amibroker sheet..."

    $ws = $wb.Worksheets.Item("Stocks_Amibroker")

    if ($ws -eq $null) {
        throw "ERROR: Sheet 'Stocks_Amibroker' not found"
    }

    $ws.Activate()

    Start-Sleep -Seconds 2

    Write-Host "Active sheet:"
    Write-Host $excel.ActiveSheet.Name


    # ========================================================
    # MACRO 1
    # Excel -> AmiBroker
    #
    # IMPORTANT:
    # Macro 1 = Main
    #
    # Main() already does:
    #     AmiBroker.SaveDatabase
    #     AmiBroker.Quit
    #     Application.Quit
    #
    # Therefore DO NOT Save / Close Excel here.
    # ========================================================

    Write-Host "Running Macro 1..."

    $excel.Run("Main")

    Start-Sleep -Seconds 2

    Write-Host "Macro 1 completed."


    # ========================================================
    # SUCCESS
    #
    # Macro 1 already closed Excel.
    # ========================================================

    Write-Host "========================================"
    Write-Host "V4 EXCEL MACRO : SUCCESS"
    Write-Host "========================================"

    exit 0
}
catch {

    Write-Host "========================================"
    Write-Host "V4 EXCEL MACRO : FAILED"
    Write-Host "ERROR:"
    Write-Host $_.Exception.Message
    Write-Host "========================================"

    if ($wb -ne $null) {
        try {
            $wb.Close($false)
        }
        catch {}
    }

    if ($excel -ne $null) {
        try {
            $excel.Quit()
        }
        catch {}
    }

    exit 1
}