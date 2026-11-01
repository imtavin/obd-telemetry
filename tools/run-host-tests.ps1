[CmdletBinding()]
param()

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$compilerCandidates = @(
    (Get-Command g++.exe -ErrorAction SilentlyContinue).Source,
    'E:/Pasta de Programas/MinGw/mingw64/bin/g++.exe',
    'C:/Strawberry/c/bin/g++.exe'
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }
$compiler = $compilerCandidates | Select-Object -First 1
$outputDirectory = Join-Path $projectRoot '.pio/host-tests'
$outputPath = Join-Path $outputDirectory 'protocol_tests.exe'

if (-not $compiler)
{
    Write-Error 'Compilador g++ nao encontrado.'
    exit 1
}

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$sources = @(
    'src/protocol/ProtocolTypes.cpp',
    'src/protocol/Crc16.cpp',
    'src/protocol/ProtocolEncoder.cpp',
    'src/protocol/ProtocolParser.cpp',
    'src/protocol/ProtocolMessages.cpp',
    'src/protocol/Sequence.cpp',
    'src/control/SessionPolicy.cpp',
    'test_host/protocol_tests.cpp'
)
$arguments = @('-std=c++11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-Isrc') + $sources + @('-o', $outputPath)

Push-Location $projectRoot
try
{
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }

    & $outputPath
    exit $LASTEXITCODE
}
finally
{
    Pop-Location
}
