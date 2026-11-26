# Compile a private installer identity and exercise installation/reinstallation/removal.
# All files and settings use a fresh build fixture; production profiles and keys are untouched.
param(
    [Parameter(Mandatory = $true)][string]$SourceDirectory,
    [Parameter(Mandatory = $true)][string]$InnoCompiler,
    [Parameter(Mandatory = $true)][string]$Version,
    [ValidateSet('CurrentUser', 'AllUsers')][string]$InstallScope = 'CurrentUser'
)
$ErrorActionPreference = 'Stop'
$scopeArgument = '/CURRENTUSER'
if ($InstallScope -eq 'AllUsers') {
    $principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
    if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'The AllUsers installer test requires an elevated PowerShell session.'
    }
    $scopeArgument = '/ALLUSERS'
}
$project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$SourceDirectory = (Resolve-Path -LiteralPath $SourceDirectory).Path
$InnoCompiler = (Resolve-Path -LiteralPath $InnoCompiler).Path
$identity = [guid]::NewGuid().ToString('N')
# Keep the private AppId short so Inno does not shorten its uninstall registry key.
$installerAppId = "penthario.Test.$identity"
$uninstallRoot = if ($InstallScope -eq 'AllUsers') { 'HKLM:' } else { 'HKCU:' }
$uninstallKey = "$uninstallRoot\Software\Microsoft\Windows\CurrentVersion\Uninstall\${installerAppId}_is1"
$fixture = Join-Path $project "build/Release/installer-test-$identity"
$installed = [System.IO.Path]::GetFullPath((Join-Path $fixture 'Application'))
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $project 'build')) + [System.IO.Path]::DirectorySeparatorChar
if (!$installed.StartsWith($buildRoot, [System.StringComparison]::OrdinalIgnoreCase)) { throw 'Installer test path escapes the build directory.' }
New-Item -ItemType Directory -Path $fixture | Out-Null
$script = Join-Path $project 'packaging/windows/penthario.iss'
& $InnoCompiler '/Q' "/DAppVersion=$Version" "/DSourceDirectory=$SourceDirectory" "/DOutputDirectory=$fixture" `
    "/DInstallerAppId=$installerAppId" "/DInstallerName=penthario Installer Test $identity" $script
if ($LASTEXITCODE -ne 0) { throw 'Private test installer compilation failed.' }
$setup = Join-Path $fixture "penthario-$Version-windows-x64-setup.exe"

# Bound silent processes so a hidden installer prompt cannot hang local or hosted tests.
function Invoke-InstallerTestProcess([string]$Executable, [string[]]$Arguments) {
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw "Timed out: $Executable" }
    if ($process.ExitCode -ne 0) { throw "Exit code $($process.ExitCode): $Executable" }
}

$settings = Join-Path $fixture 'settings.ini'
$settingsContents = '{"marker":"' + $identity + '"}'
Set-Content -LiteralPath $settings -Value $settingsContents -Encoding ASCII -NoNewline
$settingsHash = (Get-FileHash -LiteralPath $settings).Hash
try {
    Invoke-InstallerTestProcess $setup @($scopeArgument, '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOICONS', "/DIR=`"$installed`"", "/LOG=`"$fixture/install.log`"")
    $executable = Join-Path $installed 'penthario.exe'
    if (!(Test-Path -LiteralPath $executable)) { throw 'The application was not installed.' }
    if (!(Test-Path -LiteralPath $uninstallKey)) { throw 'Uninstall registration does not match the selected install scope.' }
    if ((Get-FileHash -LiteralPath $executable).Hash -ne (Get-FileHash -LiteralPath (Join-Path $SourceDirectory 'penthario.exe')).Hash) {
        throw 'Installed application differs from the validated portable application.'
    }
    $report = Join-Path $fixture 'startup.txt'
    Invoke-InstallerTestProcess $executable @('--startup-check', '--settings-file', "`"$settings`"", '--startup-report', "`"$report`"")
    if (!((Get-Content -LiteralPath $report) -contains "Application version: $Version")) { throw 'Installed version check failed.' }
    if (!((Get-Content -LiteralPath $settings) -contains $settingsContents)) { throw 'The separate settings marker was lost.' }
    # Startup checks and installer operations must preserve separate workspace data.
    $settingsHash = (Get-FileHash -LiteralPath $settings).Hash
    $query = Join-Path $installed 'user-query.sql'
    Set-Content -LiteralPath $query -Value "SELECT '$identity';" -Encoding ASCII
    $queryHash = (Get-FileHash -LiteralPath $query).Hash
    # Model an in-place rename upgrade inside the isolated application directory.
    $legacyExecutable = Join-Path $installed 'pgadmin-jr.exe'
    Set-Content -LiteralPath $legacyExecutable -Value 'superseded application' -Encoding ASCII
    $svgLibrary = Join-Path $installed 'libpq.dll'
    Remove-Item -LiteralPath $svgLibrary
    Invoke-InstallerTestProcess $setup @($scopeArgument, '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOICONS', "/DIR=`"$installed`"", "/LOG=`"$fixture/reinstall.log`"")
    if (!(Test-Path -LiteralPath $svgLibrary)) { throw 'Reinstallation did not restore an installed file.' }
    if (Test-Path -LiteralPath $legacyExecutable) { throw 'The superseded pgadmin-jr executable was not removed.' }
    if ((Get-FileHash -LiteralPath $query).Hash -ne $queryHash) { throw 'Reinstallation changed an untracked query file.' }
} finally {
    $uninstaller = Join-Path $installed 'unins000.exe'
    if (Test-Path -LiteralPath $uninstaller) {
        Invoke-InstallerTestProcess $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$fixture/uninstall.log`"")
    }
}
if (Test-Path -LiteralPath (Join-Path $installed 'penthario.exe')) { throw 'Uninstallation left the application executable.' }
if (Test-Path -LiteralPath $uninstallKey) { throw 'Uninstallation left its private registration.' }
if ((Get-FileHash -LiteralPath $query).Hash -ne $queryHash) { throw 'Uninstallation removed or changed the untracked query file.' }
if ((Get-FileHash -LiteralPath $settings).Hash -ne $settingsHash) { throw 'Installation or removal changed separate settings.' }
Write-Output "Installer $InstallScope install/reinstall/startup/uninstall checks passed: $fixture"
