# Purpose: build dist\ryb_<module>.zip for each module: what goes in a board's home directory to
# run RYB Comms with a preset, plus install_<module>.sh. Owning module: management (mgmt/).
#
# Usage:  powershell -ExecutionPolicy Bypass -File tools\make_preset_zips.ps1 [-Preset manual_chain]
# On the board:  unzip ryb_<module>.zip  (or: python3 -m zipfile -e ryb_<module>.zip .)
#                bash install_<module>.sh
param([string]$Preset = "manual_chain")

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = Split-Path -Parent $PSScriptRoot
$presetPath = Join-Path $root "include\ryb\presets\$Preset.json"
if (-not (Test-Path $presetPath)) { throw "no preset $presetPath" }
$presetText = [System.IO.File]::ReadAllText($presetPath)
$config = $presetText | ConvertFrom-Json
$template = [System.IO.File]::ReadAllText((Join-Path $root "tools\preset_install.sh.in"))
$dist = Join-Path $root "dist"
New-Item -ItemType Directory -Force $dist | Out-Null

$chain = @("heartbeat", "sound", "decision", "motor")

function Pin-Label([string]$pin) {
    if ($pin -match '^RBPI0*(\d+)$') { return "pin $($Matches[1])" }
    return $pin
}

function Rx-Pin($module) {
    $rx = @($config.modules.$module.rx)
    if ($rx.Count -gt 0) { return $rx[0].pin }
    return $null
}

# Files every board needs, relative to the repository root; the module's own directory is added
# per zip. The old prototype and its binary stay out.
function Common-Files {
    $list = @("CMakeLists.txt", "tools/deploy.sh", "tools/pinfind.sh")
    Get-ChildItem -Recurse -File (Join-Path $root "include\ryb") | ForEach-Object {
        $rel = $_.FullName.Substring($root.Length + 1).Replace('\', '/')
        if ($rel -notmatch '(client\.c|a\.out|\.bak)$' -and $rel -ne "include/ryb/ryb_comms.json") {
            $list += $rel
        }
    }
    return $list
}

function Add-Entry($zip, [string]$name, [byte[]]$bytes) {
    $entry = $zip.CreateEntry($name, [System.IO.Compression.CompressionLevel]::Optimal)
    $stream = $entry.Open()
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Close()
}

# Shell scripts must reach the board with LF line endings whatever the checkout uses.
function Text-Bytes([string]$text) {
    return [System.Text.Encoding]::UTF8.GetBytes($text.Replace("`r`n", "`n"))
}

$common = Common-Files
foreach ($module in $chain) {
    $i = [array]::IndexOf($chain, $module)
    $tx = $config.modules.$module.tx
    $rx = Rx-Pin $module
    $txText = "none (the motor ends the chain)"
    if ($tx) {
        $next = $chain[$i + 1]
        $txText = "$(Pin-Label $tx)  ->  wire to $next's RX pin $(Pin-Label (Rx-Pin $next))"
    }
    $rxText = "none (heartbeat starts the chain)"
    if ($rx) {
        $prev = $chain[$i - 1]
        $rxText = "$(Pin-Label $rx)  <-  wire from $prev's TX pin $(Pin-Label $config.modules.$prev.tx)"
    }
    $lines = @("TX $txText", "RX $rxText")
    $script = $template.Replace("@MODULE@", $module).Replace("@TX@", $txText).Replace("@RX@", $rxText)

    $zipPath = Join-Path $dist "ryb_$module.zip"
    if (Test-Path $zipPath) { Remove-Item $zipPath }
    $zip = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        Add-Entry $zip "install_$module.sh" (Text-Bytes $script)
        Add-Entry $zip "tby-team20/include/ryb/ryb_comms.json" (Text-Bytes $presetText)
        $files = $common + (Get-ChildItem -Recurse -File (Join-Path $root "modules\$module") |
            ForEach-Object { $_.FullName.Substring($root.Length + 1).Replace('\', '/') })
        foreach ($rel in $files) {
            $full = Join-Path $root $rel
            if ($rel -match '\.(sh|json|cmake|txt|c|h|in|md)$') {
                $bytes = Text-Bytes ([System.IO.File]::ReadAllText($full))
            } else {
                $bytes = [System.IO.File]::ReadAllBytes($full)
            }
            Add-Entry $zip "tby-team20/$rel" $bytes
        }
    } finally {
        $zip.Dispose()
    }
    $size = (Get-Item $zipPath).Length
    Write-Output ("{0,-22} {1,7} bytes  {2}" -f "ryb_$module.zip", $size, ($lines -join ' | ').Trim())
}
