param(
    [string] $BuildDir = "build-windows-juce",
    [string] $Config = "Release",
    [string] $Generator = "Visual Studio 17 2022",
    [string] $Architecture = "x64",
    [string] $JuceDir = "",
    [switch] $SkipTests
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $repoRoot $BuildDir
$isMultiConfig = $Generator -like "Visual Studio*"

$cmakeArgs = @(
    "-S", $repoRoot,
    "-B", $buildPath,
    "-G", $Generator,
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    "-DLIVELOOPING_BUILD_JUCE_APP=ON",
    "-DCMAKE_BUILD_TYPE=$Config"
)

if ($isMultiConfig) {
    $cmakeArgs += @("-A", $Architecture)
}

if ($JuceDir -ne "") {
    $cmakeArgs += "-DLIVELOOPING_JUCE_DIR=$JuceDir"
}

Write-Host "Configuring LiveLooping for Windows..."
cmake @cmakeArgs

Write-Host "Building LiveLooping ($Config)..."
cmake --build $buildPath --config $Config

if (-not $SkipTests) {
    Write-Host "Running tests ($Config)..."
    ctest --test-dir $buildPath -C $Config --output-on-failure
}

$exePath = Join-Path $buildPath "livelooping_product_artefacts\$Config\LiveLooping.exe"
if (-not (Test-Path $exePath)) {
    $exePath = Join-Path $buildPath "livelooping_product_artefacts\LiveLooping.exe"
}

if (-not (Test-Path $exePath)) {
    throw "Expected LiveLooping.exe was not found at: $exePath"
}

Write-Host ""
Write-Host "LiveLooping.exe:"
Write-Host $exePath
