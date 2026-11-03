# Runs the tests of CPPActressMAS by hand on Windows (MSVC): configures and builds the msvc preset (core only, no network
# transport), then runs the tests of the types given, or all of them.
#
# Usage: tests\run.ps1 [-Preset msvc] [-Configuration Debug|Release] [type...]
#   type    unit, integration, network, characterization, iso or examples (the CTest labels, tests\README.md)
#
# Examples: tests\run.ps1                    every test, Debug
#           tests\run.ps1 unit iso           the unit and oracle tests
#           tests\run.ps1 -Configuration Release
# Preset and Configuration are named only: otherwise "unit iso" would bind to them by position
[CmdletBinding(PositionalBinding = $false)]
param(
	[string]$Preset = "msvc",
	[ValidateSet("Debug", "Release", "RelWithDebInfo")][string]$Configuration = "Debug",
	[Parameter(ValueFromRemainingArguments = $true)][string[]]$Types = @()
)
$ErrorActionPreference = "Stop"

$s_types = @("unit", "integration", "network", "characterization", "iso", "examples")
foreach ($l_type in $Types) {
	if ($s_types -notcontains $l_type) {
		Write-Error "unknown type: $l_type (types: $($s_types -join ' '))"
	}
}

Set-Location (Split-Path -Parent $PSScriptRoot)
cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset $Preset --config $Configuration
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($Types.Count -eq 0) {
	ctest --preset $Preset -C $Configuration
} else {
	ctest --preset $Preset -C $Configuration -L "^($($Types -join '|'))$"
}
exit $LASTEXITCODE
