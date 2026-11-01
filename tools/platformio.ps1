[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $PlatformIoArguments
)

if (-not $PlatformIoArguments)
{
    $PlatformIoArguments = @('run')
}

$projectPath = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectParentPath = Split-Path -Path $projectPath -Parent
$projectName = Split-Path -Path $projectPath -Leaf
$driveLetter = 'T:'
$mappedProjectPath = $driveLetter + '/' + $projectName
$pioCandidates = @(
    (Get-Command pio.exe -ErrorAction SilentlyContinue).Source,
    (Join-Path $env:USERPROFILE '.platformio/penv/Scripts/pio.exe')
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }
$pioPath = $pioCandidates | Select-Object -First 1
$mappingCreated = $false
$exitCode = 1

try
{
    if (-not $pioPath)
    {
        throw 'PlatformIO nao encontrado. Instale o PlatformIO Core ou use a extensao do VS Code.'
    }

    # Do not destroy a drive or mapping owned by the user.
    $existingMappings = (& subst.exe 2>$null | Out-String)
    $existingMapping = $existingMappings -split "`n" | Where-Object { $_ -like "$driveLetter*" } | Out-String
    if ($existingMapping -or (Test-Path -LiteralPath ($driveLetter + '\')))
    {
        throw "A unidade $driveLetter ja esta em uso. Libere-a ou ajuste o wrapper antes de executar o PlatformIO."
    }

    & subst.exe $driveLetter $projectParentPath
    if ($LASTEXITCODE -ne 0)
    {
        throw 'Nao foi possivel criar a unidade virtual sem espacos.'
    }
    $mappingCreated = $true

    Set-Location -LiteralPath $mappedProjectPath
    & $pioPath @PlatformIoArguments
    $exitCode = $LASTEXITCODE
}
catch
{
    Write-Error $_
    $exitCode = 1
}
finally
{
    if ($mappingCreated)
    {
        & subst.exe /d $driveLetter *> $null
    }
}

exit $exitCode
