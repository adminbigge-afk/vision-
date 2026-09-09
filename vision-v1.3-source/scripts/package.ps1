param([string]$Version = '1.3')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    if ($Version -ne '1.3') { throw 'Update CMake and installer version metadata before changing the release version.' }
    $payloadRoot = Join-Path $projectRoot "build\package-v$Version"
    $binaryRoot = Join-Path $projectRoot 'build\windows-release'
    $qtRoot = Join-Path $projectRoot '.deps\Qt\6.8.3\msvc2022_64'
    New-Item -ItemType Directory -Path $payloadRoot -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $binaryRoot 'vision.exe') -Destination $payloadRoot
    Copy-Item -LiteralPath '.deps\mpv\libmpv-2.dll' -Destination $payloadRoot
    foreach ($name in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Sql.dll')) {
        Copy-Item -LiteralPath (Join-Path $qtRoot "bin\$name") -Destination $payloadRoot
    }
    New-Item -ItemType Directory -Path (Join-Path $payloadRoot 'platforms'),(Join-Path $payloadRoot 'sqldrivers') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $qtRoot 'plugins\platforms\qwindows.dll') -Destination (Join-Path $payloadRoot 'platforms')
    Copy-Item -LiteralPath (Join-Path $qtRoot 'plugins\sqldrivers\qsqlite.dll') -Destination (Join-Path $payloadRoot 'sqldrivers')
    Copy-Item -LiteralPath '.deps\vulkan\VulkanRT-X64-1.4.357.0-Components\x64\vulkan-1.dll' -Destination $payloadRoot
    Get-ChildItem -LiteralPath 'C:\BuildTools2022\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT' -Filter '*.dll' |
        Copy-Item -Destination $payloadRoot
    Copy-Item -LiteralPath 'licenses' -Destination $payloadRoot -Recurse -Force
    Copy-Item -LiteralPath 'docs\使用与测试说明.md' -Destination $payloadRoot
    Copy-Item -LiteralPath 'CHANGELOG.md' -Destination $payloadRoot
    @'
[Paths]
Plugins=.
Translations=translations
'@ | Set-Content -LiteralPath (Join-Path $payloadRoot 'qt.conf') -Encoding utf8

    $manifest = Get-ChildItem -LiteralPath $payloadRoot -Recurse -File | Where-Object { $_.Name -ne 'payload-manifest.json' } | ForEach-Object {
        [pscustomobject]@{file=[IO.Path]::GetRelativePath($payloadRoot,$_.FullName);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $payloadRoot 'payload-manifest.json') -Encoding utf8

    $uninstallFile = Join-Path $projectRoot 'build\uninstall-files.nsh'
    $uninstallLines = @(Get-ChildItem -LiteralPath $payloadRoot -Recurse -File | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($payloadRoot, $_.FullName)
        'Delete "$INSTDIR\' + $relative + '"'
    })
    $uninstallLines += Get-ChildItem -LiteralPath $payloadRoot -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
        'RMDir "$INSTDIR\' + [IO.Path]::GetRelativePath($payloadRoot, $_.FullName) + '"'
    }
    $uninstallLines | Set-Content -LiteralPath $uninstallFile -Encoding utf8
    $output = Join-Path $projectRoot "dist\vision-v$Version-setup-x64.exe"
    & '.tools\nsis\nsis-3.08\makensis.exe' /INPUTCHARSET UTF8 "/DSTAGING=$payloadRoot" "/DOUTPUT=$output" "/DICON=$projectRoot\resources\windows\vision.ico" "/DUNINSTALL_FILES=$uninstallFile" 'packaging\installer.nsi'
    if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed' }
    $hash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
    "$hash  vision-v$Version-setup-x64.exe" | Set-Content -LiteralPath "$output.sha256" -Encoding ascii
} finally { Pop-Location }
