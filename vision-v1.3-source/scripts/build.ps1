param(
    [string]$QtRoot = (Join-Path $PSScriptRoot '..\.deps\Qt\6.8.3\msvc2022_64'),
    [string]$VisualStudioRoot = 'C:\BuildTools2022',
    [switch]$Fresh
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$vcvars = Join-Path $VisualStudioRoot 'VC\Auxiliary\Build\vcvars64.bat'
$cmake = Join-Path $VisualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninjaDir = Join-Path $VisualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
foreach ($required in @($vcvars, $cmake, (Join-Path $ninjaDir 'ninja.exe'), (Join-Path $QtRoot 'lib\cmake\Qt6\Qt6Config.cmake'))) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing build dependency: $required" }
}

# Import the existing MSVC environment in this process only; no persistent PATH changes.
$previousEnvironment = @{}
Get-ChildItem Env: | ForEach-Object { $previousEnvironment[$_.Name] = $_.Value }
Push-Location $projectRoot
try {
    $compilerEnvironment = & $env:ComSpec /d /s /c "`"`"$vcvars`" >nul && set`""
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed' }
    foreach ($line in $compilerEnvironment) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
    $env:PATH = "$ninjaDir;$env:PATH"
    $env:VSLANG = '1033'
    $env:VISION_QT_ROOT = (Resolve-Path -LiteralPath $QtRoot).Path
    $compiler = Join-Path $env:VCToolsInstallDir 'bin\Hostx64\x64\cl.exe'
    $sdkBin = (Join-Path $env:WindowsSdkVerBinPath 'x64').Replace('\', '/')
    $configureArgs = @('--preset', 'windows-release')
    if ($Fresh) { $configureArgs += '--fresh' }
    & $cmake @configureArgs "-DCMAKE_CXX_COMPILER=$compiler" `
        "-DCMAKE_MAKE_PROGRAM=$ninjaDir\ninja.exe" `
        "-DCMAKE_RC_COMPILER:FILEPATH=$sdkBin/rc.exe" "-DCMAKE_MT:FILEPATH=$sdkBin/mt.exe"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & $cmake --build --preset windows-release
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
} finally {
    Pop-Location
    Get-ChildItem Env: | Where-Object { -not $previousEnvironment.ContainsKey($_.Name) } |
        ForEach-Object { [Environment]::SetEnvironmentVariable($_.Name, $null, 'Process') }
    foreach ($name in $previousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $previousEnvironment[$name], 'Process')
    }
}
