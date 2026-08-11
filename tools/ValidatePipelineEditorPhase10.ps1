param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [switch]$SkipGpu
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$bin = Join-Path $root "pipeline-editor/build/vs2026/bin/x64/$Configuration"
$editor = Join-Path $bin 'PipelineEditor.exe'
$resources = Join-Path $root 'resources'
$editorResources = Join-Path $resources 'pipeline-editor'
$editorIni = Join-Path $bin 'editor.ini'

function Require-File([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing deployed file: $path" }
}
function Run-Editor([string[]]$arguments, [int]$expected, [string]$code = '') {
    $out = [IO.Path]::GetTempFileName()
    $err = [IO.Path]::GetTempFileName()
    try {
        $process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $bin -Wait -PassThru -NoNewWindow -RedirectStandardOutput $out -RedirectStandardError $err
        $text = (Get-Content $out -Raw -ErrorAction SilentlyContinue) + (Get-Content $err -Raw -ErrorAction SilentlyContinue)
        if ($process.ExitCode -ne $expected) { throw "PipelineEditor exited $($process.ExitCode), expected $expected.`n$text" }
        if ($code -and $text -notmatch [regex]::Escape($code)) { throw "Expected diagnostic $code.`n$text" }
    } finally { Remove-Item $out,$err -Force -ErrorAction SilentlyContinue }
}

Require-File $editor
$sdlRuntime = if ($Configuration -eq 'Debug') { 'SDL3d.dll' } else { 'SDL3.dll' }
$glewRuntime = if ($Configuration -eq 'Debug') { 'glew32d.dll' } else { 'glew32.dll' }
foreach ($runtime in @($sdlRuntime,$glewRuntime)) { Require-File (Join-Path $bin $runtime) }
Require-File $editorIni
if (Test-Path -LiteralPath (Join-Path $bin 'resources')) { throw 'PipelineEditor output must not contain a copied resources directory.' }
if ((Get-Content -LiteralPath $editorIni -Raw) -notmatch 'resourcesLocation\s*=') { throw 'Deployed editor.ini does not define resourcesLocation.' }
Require-File (Join-Path $resources 'pipeline-editor/fa-solid-900.ttf')
$dumpbin = Get-ChildItem "${env:ProgramFiles}/Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
if ($dumpbin) {
    $headers = & $dumpbin.FullName /headers $editor 2>&1 | Out-String
    if ($headers -notmatch '8664 machine \(x64\)') { throw 'PipelineEditor deployment is not an x64 PE image.' }
    $exports = & $dumpbin.FullName /exports (Join-Path $bin ($(if ($Configuration -eq 'Debug') {'MppResourceParsersd.dll'} else {'MppResourceParsers.dll'}))) 2>&1 | Out-String
    if ($exports -match 'ResourceQuality|getQuality|setQuality') { throw 'Removed resource-quality ABI symbol was reintroduced.' }
}
foreach ($asset in @(
    'shared/pbr/arrow.png',
    'shared/pbr/DefaultPbrPreview.scene.xml',
    'shared/pbr/PbrPreviewResources.xml',
    'shared/pbr/templates/Minimal.pipeline.xml',
    'shared/pbr/templates/Shadows.pipeline.xml',
    'shared/pbr/templates/Full.pipeline.xml',
    'shared/pbr/templates/Empty.pipeline.xml')) { Require-File (Join-Path $resources $asset) }

# Every shipped source XML must be well formed before parser-level validation.
Get-ChildItem (Join-Path $resources 'shared'),$editorResources -Recurse -Filter *.xml | ForEach-Object {
    try { [void][xml](Get-Content -LiteralPath $_.FullName -Raw) }
    catch { throw "Malformed XML: $($_.FullName): $($_.Exception.Message)" }
}

Run-Editor @('--validate') 2
foreach ($template in @('Minimal','Shadows','Full','Empty')) {
    Run-Editor @('--validate', (Join-Path $resources "shared/pbr/templates/$template.pipeline.xml")) 0
}
Run-Editor @('--validate','--warnings-as-errors',(Join-Path $resources 'shared/pbr/templates/Empty.pipeline.xml')) 1 'MPP-PIPELINE-008'

$invalid = @{
    'MissingOptionalFallback.pipeline.xml' = 'MPP-PIPELINE-011'
    'InvalidLocalSampler.pipeline.xml' = 'MPP-PIPELINE-RESOURCE-001'
    'UnknownCoreField.pipeline.xml' = 'MPP-PIPELINE-CLI-002'
    'UnsupportedVersion.pipeline.xml' = 'MPP-PIPELINE-001'
    'InvalidGraphReference.pipeline.xml' = 'MPP-PIPELINE-CLI-002'
    'InvalidScene.pipeline.xml' = 'MPP-SCENE-007'
}
foreach ($fixture in $invalid.GetEnumerator()) {
    Run-Editor @('--validate', (Join-Path $editorResources "invalid/$($fixture.Key)")) 1 $fixture.Value
}

# Reject accidentally reintroduced multi-definition/quality-era XML in native fixtures.
$legacy = Get-ChildItem (Join-Path $resources 'shared'),$editorResources -Recurse -Filter *.xml |
    Select-String -Pattern '<quality>|<qualities>|RSE2|RSER' -CaseSensitive:$false
if ($legacy) { throw "Legacy quality/multi-definition marker found: $($legacy.Path -join ', ')" }

if (-not $SkipGpu) {
    # No positional document must open the Full template as a valid untitled workspace.
    Run-Editor @('--smoke-test') 0
    foreach ($template in @('Minimal','Shadows','Full','Empty')) {
        Run-Editor @('--smoke-test', (Join-Path $resources "shared/pbr/templates/$template.pipeline.xml")) 0
    }
}

Write-Host "PipelineEditor Phase 10 $Configuration validation passed."
