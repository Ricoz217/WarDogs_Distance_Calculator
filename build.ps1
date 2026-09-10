[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$Package
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $visualStudio) {
    throw '找不到带“使用 C++ 的桌面开发”组件的 Visual Studio 2022。'
}

$developerPrompt = Join-Path $visualStudio 'Common7\Tools\VsDevCmd.bat'
$cmake = Join-Path $visualStudio 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $visualStudio 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$qtRoot = if ($env:QT_ROOT) { $env:QT_ROOT } else { 'D:\Qt\6.8.3\msvc2022_64' }
$qtCmake = Join-Path $qtRoot 'lib\cmake'
$qtDeploy = Join-Path $qtRoot 'bin\windeployqt.exe'
if (-not (Test-Path $qtCmake) -or -not (Test-Path $qtDeploy)) {
    throw "找不到 Qt 6 C++ 开发环境：$qtRoot"
}
$buildDirectory = Join-Path $projectRoot "out\$($Configuration.ToLowerInvariant())"
$installDirectory = Join-Path $projectRoot "out\package"

if ($Package -and (Test-Path $installDirectory)) {
    $resolvedProject = [IO.Path]::GetFullPath($projectRoot).TrimEnd('\') + '\'
    $resolvedInstall = [IO.Path]::GetFullPath($installDirectory)
    if (-not $resolvedInstall.StartsWith($resolvedProject, [StringComparison]::OrdinalIgnoreCase)) {
        throw "拒绝清理项目目录之外的路径：$resolvedInstall"
    }
    Remove-Item -LiteralPath $resolvedInstall -Recurse -Force
}

$commands = @(
    'call "{0}" -arch=x64 -host_arch=x64' -f $developerPrompt
    '"{0}" -S "{1}" -B "{2}" -G Ninja -DCMAKE_MAKE_PROGRAM="{3}" -DCMAKE_BUILD_TYPE={4} -DCMAKE_PREFIX_PATH="{5}"' -f $cmake, $projectRoot, $buildDirectory, $ninja, $Configuration, $qtCmake
    '"{0}" --build "{1}"' -f $cmake, $buildDirectory
    '"{0}" --test-dir "{1}" --output-on-failure' -f (Join-Path (Split-Path $cmake) 'ctest.exe'), $buildDirectory
)
if ($Package) {
    $commands += '"{0}" --install "{1}" --prefix "{2}"' -f $cmake, $buildDirectory, $installDirectory
}

& $env:ComSpec /d /s /c ($commands -join ' && ')
if ($LASTEXITCODE -ne 0) {
    throw "构建失败，退出码 $LASTEXITCODE"
}

if ($Package) {
    & $qtDeploy --release --no-translations --no-opengl-sw `
        --no-system-d3d-compiler `
        --skip-plugin-types generic,iconengines,imageformats,networkinformation,styles,tls `
        --dir $installDirectory `
        (Join-Path $installDirectory 'WarDogsDistanceCalculator.exe')
    if ($LASTEXITCODE -ne 0) {
        throw "Qt 运行库部署失败，退出码 $LASTEXITCODE"
    }
    $qtLicenseDirectory = Join-Path $installDirectory 'licenses\qt'
    New-Item -ItemType Directory -Path $qtLicenseDirectory -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot 'third_party\qt\LGPL-3.0.txt') `
        -Destination $qtLicenseDirectory
    Copy-Item -LiteralPath (Join-Path $projectRoot 'third_party\qt\GPL-3.0.txt') `
        -Destination $qtLicenseDirectory
    $archive = Join-Path $projectRoot 'out\WarDogsDistanceCalculator-win-x64.zip'
    Compress-Archive -Path (Join-Path $installDirectory '*') -DestinationPath $archive -Force
    Write-Host "已生成：$archive"
} else {
    Write-Host "程序位于：$(Join-Path $buildDirectory 'WarDogsDistanceCalculator.exe')"
}
