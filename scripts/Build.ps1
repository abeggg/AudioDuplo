param([string]$BuildDirectory = 'build', [switch]$Package)
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory = Join-Path $root $BuildDirectory }
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vs = & $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vs) { $cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }
    }
}
if (-not $cmake -or -not (Test-Path -LiteralPath $cmake)) { throw 'Install Visual Studio C++ Build Tools, Windows SDK and CMake.' }
function Invoke-CMake { & $cmake @args; if ($LASTEXITCODE -ne 0) { throw "CMake failed ($LASTEXITCODE)." } }
Invoke-CMake -S $root -B $BuildDirectory -A x64 -DBUILD_TESTING=ON
Invoke-CMake --build $BuildDirectory --config Release --parallel
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
& $ctest --test-dir $BuildDirectory -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
if ($Package) {
    $stage = Join-Path $BuildDirectory 'portable'
    Invoke-CMake --install $BuildDirectory --config Release --prefix $stage
    $zip = Join-Path $BuildDirectory 'AudioDuplo-1.1.4-windows-x64.zip'
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
    $hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  AudioDuplo-1.1.4-windows-x64.zip" | Set-Content (Join-Path $BuildDirectory 'SHA256SUMS.txt') -Encoding ascii
    Write-Host "Portable package: $zip"
}
