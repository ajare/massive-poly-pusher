param(
    [string]$Configuration = "Debug",
    [string]$BuildDirectory = (Join-Path $PSScriptRoot "../build")
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$editor = Join-Path $BuildDirectory "bin/$Configuration/ParticleEditor.exe"
$demo = Join-Path $BuildDirectory "demo-suite/$Configuration/DemoSuite.exe"
if (-not (Test-Path $demo)) { $demo = Join-Path $BuildDirectory "bin/$Configuration/DemoSuite.exe" }
$sample = Join-Path $root "resources/demo-suite/res/SerializedParticleEffect.particle.yaml"

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    Write-Host "> $Program $Arguments"
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code ${LASTEXITCODE}: $Program $Arguments" }
}

if (-not (Test-Path $editor)) { throw "ParticleEditor was not built: $editor" }
Invoke-Checked $editor @("--validate", $sample)
Invoke-Checked $editor @("--core-particle-tests")
foreach ($suite in @("--document-tests", "--preview-tests", "--control-tests", "--resource-tests", "--spatial-tests")) {
    Invoke-Checked $editor @($suite)
}
Invoke-Checked $editor @("--smoke-test", $sample)
if (-not (Test-Path $demo)) { throw "DemoSuite was not built: $demo" }
Invoke-Checked $demo @("--particle-tests")
Write-Host "Particle Editor acceptance validation passed."
