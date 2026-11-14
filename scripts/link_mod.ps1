# Links this repo's mod folder into the SatisfactoryModLoader starter project,
# so you edit files here and Unreal sees them in its Mods/ folder.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\link_mod.ps1 -SmlProject "C:\Modding\SatisfactoryModLoader"

param(
    [Parameter(Mandatory = $true)]
    [string]$SmlProject
)

$source = Resolve-Path (Join-Path $PSScriptRoot "..\Mod\Splerger")
$modsDir = Join-Path $SmlProject "Mods"
$target = Join-Path $modsDir "Splerger"

if (-not (Test-Path (Join-Path $SmlProject "FactoryGame.uproject"))) {
    Write-Error "No FactoryGame.uproject in '$SmlProject' - point -SmlProject at the cloned SatisfactoryModLoader folder."
    exit 1
}
if (Test-Path $target) {
    Write-Error "'$target' already exists. Remove it first if you want to re-link."
    exit 1
}

# A directory junction needs no admin rights.
New-Item -ItemType Junction -Path $target -Target $source | Out-Null
Write-Host "Linked $target -> $source"
Write-Host "Now right-click FactoryGame.uproject > Generate Visual Studio project files."
