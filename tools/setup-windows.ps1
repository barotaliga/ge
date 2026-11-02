# Brings a Windows machine (10 or 11, x64) up to what the tree needs: builds the engine and Fall
# Line with MSVC on the release preset, as CI's Windows row does, runs the whole suite (the GPU
# tests on the machine's own D3D12 adapter), and has the tools the gates and tools/*.sh call:
# clang-format 18, Node.js, Python 3, gh, GNU make and Git Bash. The one list of Windows tools, as
# tools/setup-ubuntu.sh is Linux's and tools/setup-macos.sh the Mac's.
#
#   tools\setup-windows.ps1            install or upgrade what is missing, broken or too old
#   tools\setup-windows.ps1 --check    say what is missing, broken or too old, change nothing (exit 1)
#   tools\setup-windows.ps1 --list     print the winget packages it asks for, one per line
#   --quiet                            say nothing when there is nothing to do
#   tools\setup-windows.ps1 --verify   install, then build and run the whole suite on the release
#                                      preset in MSVC's x64 environment (GE_GPU_REQUIRE=1: a GPU
#                                      test that skips fails)
#
# RUN IT AS YOURSELF, FROM AN ORDINARY (NOT ELEVATED) POWERSHELL. The installers that need an
# administrator ask for it themselves (a UAC prompt); Python and the PATH entries go to your own
# account. Windows' default execution policy runs no script, so call it through:
#   powershell -ExecutionPolicy Bypass -File tools\setup-windows.ps1 [--check|--verify|...]
# (from Git Bash: powershell.exe -ExecutionPolicy Bypass -File tools/setup-windows.ps1). It runs
# on the Windows PowerShell 5.1 every Windows 10 and 11 has, and on PowerShell 7.
#
# What it brings, and how:
#   1. MSVC: Visual Studio 2026's Build Tools with the C++ workload (MSVC, the Windows SDK), by
#      winget; an existing Build Tools without the workload is modified to add it, and an MSVC
#      without a Windows SDK gets one. Any Visual Studio 2026 or later that has MSVC counts (the
#      prebuilt Dawn links against its STL, and 2022's lacks symbols it calls). MSVC is the only
#      Windows compiler the tree is tested with;
#   2. by winget (https://aka.ms/getwinget, part of Windows 11 and of Windows 10's App Installer):
#      Git for Windows (git, and Git Bash: tools/*.sh and the Makefile's recipes run in it), CMake
#      3.25+, Ninja (the presets' generator), ccache 4.10+ (CI's Windows row pins 4.10.2; it caches
#      MSVC's compiles, cmake/CompilerCache.cmake), GNU make 4+, LLVM 18 for clang-format and
#      clang-tidy (CI's major: another clang-format formats some lines differently and fails
#      fmt-check), Python 3.10+ (tools/release.py), Node.js 18+ (tools/*.mjs), gh, and nasm (the
#      AV1 encoder's x86 assembly);
#   3. PATH: a tool its installer left off PATH (LLVM, NASM) gets its directory put first in your
#      user PATH, and `python3` (what the Makefile and tools/precheck.sh call) is made a real
#      Python: python.org's installer has only python.exe, and Windows' own python3.exe is an
#      alias that opens the Microsoft Store, so python3.exe is written beside python.exe;
#   4. the checkout: a tools/*.sh checked out with CRLF line endings (Git for Windows' default,
#      core.autocrlf=true, before .gitattributes said `*.sh eol=lf`) stops bash at its first line,
#      so each one with no change of yours is checked out again with LF.
# SDL3, Dawn, libaom and libopus are not installed here: CMake downloads them, pinned and
# checksummed, into .cache\deps\ on the first configure. Blender (`ge asset`) is optional and not
# installed here (GE_BLENDER, the `assets` skill).
#
# BUILDING AFTERWARDS: in MSVC's x64 environment ("x64 Native Tools Command Prompt for VS",
# or the Launch-VsDevShell.ps1 line printed at the end), on the release preset only (the prebuilt
# Dawn is a release library; the sanitizers are GCC's and Clang's); Git Bash started from it runs make.
#
# IDEMPOTENT AND FAST WHEN DONE. A tool counts as present when the command on PATH runs and
# reports at least the minimum, wherever it came from; one that is on PATH but fails to run counts
# as broken and is reinstalled. Only what is missing, broken or too old is touched; when nothing
# is, the script calls neither winget nor the network.
#
# ge:summary Takes a Windows machine to one that builds and tests the tree with MSVC: Visual
# ge:summary Studio's Build Tools and every tool, by winget; a no-op when all are there
# ge:keywords setup windows winget msvc visual-studio build-tools vswhere dependencies install
# ge:keywords toolchain bootstrap idempotent clang-format git-bash fresh-install
# ge:skill build

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$Argv = @($args)

function Say([string]$Message) { if (-not $script:Quiet) { Write-Host $Message } }
function Warn([string]$Message) { [Console]::Error.WriteLine("setup-windows: $Message") }
function Fail([string]$Message) { Warn $Message; exit 1 }

function Show-Help {
  foreach ($line in (Get-Content -LiteralPath $PSCommandPath)) {
    if ($line -notmatch '^#') { break }
    if ($line -match '^# ge:summary') { break }
    Write-Host ($line -replace '^# ?', '')
  }
}

$Mode = 'install'
$Quiet = $false
foreach ($a in $Argv) {
  switch -Regex ($a) {
    '^--?check$' { $Mode = 'check'; break }
    '^--?list$' { $Mode = 'list'; break }
    '^--?verify$' { $Mode = 'verify'; break }
    '^--?quiet$' { $Quiet = $true; break }
    '^(--?h|--?help|/\?)$' { Show-Help; exit 0 }
    default {
      Warn "unknown flag '$a'; use --check, --list, --verify, --quiet or none (--help)"
      exit 2
    }
  }
}

if ([Environment]::OSVersion.Platform -ne 'Win32NT') {
  Fail 'this is not Windows; on Ubuntu run sudo tools/setup-ubuntu.sh, on a Mac tools/setup-macos.sh'
}
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location -LiteralPath $Root
$X64 = $env:PROCESSOR_ARCHITECTURE -eq 'AMD64' -or $env:PROCESSOR_ARCHITEW6432 -eq 'AMD64'

# One row a tool. Present when `<Cmd>.exe` on PATH runs and reports at least Min (Exact: that
# major and no other). Id is the winget package (Version pins it); Dirs are where its installer
# puts it, for when it is installed but not on PATH.
function Tool {
  param([string]$Cmd, [string]$Min, [string]$Id, [switch]$Exact, [string]$Version = '',
    [string]$Override = '', [string[]]$Dirs = @(), [string]$VersionArg = '--version', [string]$Scope = '')
  [pscustomobject]@{
    Cmd = $Cmd; Min = $Min; Id = $Id; Exact = [bool]$Exact; Version = $Version
    Override = $Override; Dirs = @($Dirs); VersionArg = $VersionArg; Scope = $Scope
  }
}
# The 64-bit Program Files, also from a 32-bit PowerShell. Versions are quoted: unquoted, 3.10 is
# the number 3.1.
$PF = $env:ProgramFiles
if ($env:ProgramW6432) { $PF = $env:ProgramW6432 }
$Tools = @(
  (Tool 'git' '2.25' 'Git.Git' -Dirs @("$PF\Git\cmd")),
  (Tool 'cmake' '3.25' 'Kitware.CMake' -Dirs @("$PF\CMake\bin")),
  (Tool 'ninja' '1.10' 'Ninja-build.Ninja'),
  (Tool 'ccache' '4.10' 'Ccache.Ccache'),
  (Tool 'make' '4.0' 'ezwinports.make'),
  (Tool 'clang-format' '18' 'LLVM.LLVM' -Exact -Version '18.1.8' -Dirs @("$PF\LLVM\bin")),
  (Tool 'clang-tidy' '18' 'LLVM.LLVM' -Exact -Version '18.1.8' -Dirs @("$PF\LLVM\bin")),
  # python.org's installer, for this account, on PATH ahead of the Store's alias.
  (Tool 'python3' '3.10' 'Python.Python.3.12' -Scope 'user' `
    -Override '/quiet InstallAllUsers=0 PrependPath=1 Include_launcher=1 InstallLauncherAllUsers=0' `
    -Dirs @("$env:LOCALAPPDATA\Programs\Python\Python312")),
  (Tool 'node' '18' 'OpenJS.NodeJS.LTS' -Dirs @("$PF\nodejs")),
  (Tool 'gh' '2.0' 'GitHub.cli' -Dirs @("$PF\GitHub CLI"))
)
if ($X64) {
  # NASM 2: libaom's configure refuses NASM 3 (its `-hf` help no longer lists -Ox).
  $Tools += (Tool 'nasm' '2' 'NASM.NASM' -Exact -Version '2.16.03' -VersionArg '-v' -Dirs @("$PF\NASM", "$env:LOCALAPPDATA\bin\NASM"))
}
# Visual Studio 2026's Build Tools and the C++ workload (MSVC, the Windows SDK, CMake's MSVC bits).
$VsId = 'Microsoft.VisualStudio.BuildTools'
$VsWorkload = @('--add', 'Microsoft.VisualStudio.Workload.VCTools', '--includeRecommended')
$VsSdk = 'Microsoft.VisualStudio.Component.Windows11SDK.26100'
$PF86 = ${env:ProgramFiles(x86)}
if (-not $PF86) { $PF86 = $PF }  # a 32-bit Windows; the x64 note below says it is untested
$VsInstallerDir = Join-Path $PF86 'Microsoft Visual Studio\Installer'
$VsWhere = Join-Path $VsInstallerDir 'vswhere.exe'
$LlvmDir = "$PF\LLVM\bin"
$Symlinks = @('CLAUDE.md', 'GEMINI.md', '.cursorrules', '.github\copilot-instructions.md',
  '.claude\skills', '.gemini\skills')

if ($Mode -eq 'list') {
  $VsId
  $Tools | ForEach-Object { $_.Id } | Select-Object -Unique
  exit 0
}

# ---- Looking -------------------------------------------------------------------------------------

function Test-StoreAlias([string]$Path) { $Path -like '*\Microsoft\WindowsApps\*' }

# The first `<Name>.exe` on PATH; Windows' Store aliases for Python do not count.
function Find-Exe([string]$Name) {
  foreach ($c in @(Get-Command "$Name.exe" -CommandType Application -All -ErrorAction SilentlyContinue)) {
    if ($Name -like 'python*' -and (Test-StoreAlias $c.Source)) { continue }
    return $c.Source
  }
  return $null
}

# The first dotted version in a tool's own report of itself; empty when it fails to run.
function Get-ToolVersion([string]$Exe, [string]$Arg = '--version') {
  if (-not $Exe) { return '' }
  $ErrorActionPreference = 'Continue'
  try {
    $out = (@(& $Exe $Arg 2>&1) | Select-Object -First 3 | ForEach-Object { "$_" }) -join "`n"
    if ($LASTEXITCODE -ne 0) { return '' }
  } catch { return '' }
  $m = [regex]::Match($out, '[0-9]+(\.[0-9]+)+')
  if ($m.Success) { return $m.Value }
  return ''
}

# True when version Have is at least Min (dotted, numeric fields).
function Test-AtLeast([string]$Have, [string]$Min) {
  $a = $Have.Split('.')
  $b = $Min.Split('.')
  for ($i = 0; $i -lt $b.Count; $i++) {
    $x = 0
    if ($i -lt $a.Count) { $x = [int64]$a[$i] }
    $y = [int64]$b[$i]
    if ($x -gt $y) { return $true }
    if ($x -lt $y) { return $false }
  }
  return $true
}

function Test-ToolOk([string]$Have, $T) {
  if (-not $Have) { return $false }
  if ($T.Exact) { return $Have.Split('.')[0] -eq $T.Min.Split('.')[0] }
  return Test-AtLeast $Have $T.Min
}

function Get-PathDirs { @($env:Path -split ';' | Where-Object { $_ } | ForEach-Object { $_.Trim().TrimEnd('\') }) }
function Test-OnPath([string]$Dir) { @(Get-PathDirs) -contains $Dir.TrimEnd('\') }

# This process's PATH again from the registry (machine, then user), as a new terminal gets it,
# with winget's directory of portable commands.
function Update-SessionPath {
  $m = [Environment]::GetEnvironmentVariable('Path', 'Machine')
  $u = [Environment]::GetEnvironmentVariable('Path', 'User')
  $env:Path = (@($m, $u) | Where-Object { $_ }) -join ';'
  $links = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Links'
  if ((Test-Path -LiteralPath $links) -and -not (Test-OnPath $links)) { $env:Path = "$env:Path;$links" }
}

# The newest instance vswhere finds for these arguments (-latest), or $null.
function Invoke-VsWhere([string[]]$VsArgs) {
  if (-not (Test-Path -LiteralPath $VsWhere)) { return $null }
  # Windows PowerShell turns a native command's stderr into errors, which 'Stop' would throw.
  $ErrorActionPreference = 'Continue'
  $json = @(& $VsWhere @VsArgs -latest -format json -utf8 2>$null)
  if ($LASTEXITCODE -ne 0 -or -not $json.Count) { return $null }
  # Into a variable first: Windows PowerShell's ConvertFrom-Json emits a JSON array as one object.
  $parsed = ($json -join "`n") | ConvertFrom-Json
  $vs = @($parsed)
  if ($vs.Count -eq 0) { return $null }
  return $vs[0]
}

# The newest Visual Studio 2026+ with MSVC's x64 tools, or $null.
function Find-Msvc {
  $vs = Invoke-VsWhere @('-products', '*', '-version', '18.0',
    '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64')
  if (-not $vs) { return $null }
  if (-not (Test-Path -LiteralPath (Join-Path $vs.installationPath 'VC\Auxiliary\Build\vcvars64.bat'))) {
    return $null
  }
  return $vs
}

# A Build Tools 2026+ without MSVC, to modify rather than install beside; or $null.
function Find-BuildTools {
  Invoke-VsWhere @('-products', 'Microsoft.VisualStudio.Product.BuildTools', '-version', '18.0')
}

# True when a Windows 10/11 SDK's headers are installed.
function Test-WinSdk {
  foreach ($key in @('HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots',
      'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots')) {
    $p = Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
    if (-not $p -or -not ($p.PSObject.Properties.Name -contains 'KitsRoot10')) { continue }
    $inc = Join-Path $p.KitsRoot10 'Include'
    if (-not (Test-Path -LiteralPath $inc)) { continue }
    foreach ($d in @(Get-ChildItem -LiteralPath $inc -Directory -ErrorAction SilentlyContinue)) {
      if (Test-Path -LiteralPath (Join-Path $d.FullName 'um\windows.h')) { return $true }
    }
  }
  return $false
}

# Git for Windows' bash.exe beside the git on PATH (Git\cmd\git.exe, Git\bin\git.exe or
# Git\mingw64\bin\git.exe), never Windows' own bash.exe, which starts WSL.
function Find-GitBash([string]$Git) {
  $d = Split-Path -Parent $Git
  for ($i = 0; $i -lt 3 -and $d; $i++) {
    $b = Join-Path $d 'bin\bash.exe'
    if (Test-Path -LiteralPath $b) { return $b }
    $d = Split-Path -Parent $d
  }
  return $null
}

# True when this account can write in Dir (python3.exe goes beside python.exe, unelevated).
function Test-Writable([string]$Dir) {
  $probe = Join-Path $Dir "ge-setup-$PID.tmp"
  try { [IO.File]::WriteAllText($probe, ''); Remove-Item -LiteralPath $probe -Force; return $true }
  catch { return $false }
}

# A python.exe of at least Min that `python3` could be, in a directory this account can write
# python3.exe into: on PATH, where python.org's installer puts it for this account, or what the py
# launcher picks. An all-users Python in Program Files does not count (it would need elevation).
function Find-Python([string]$Min) {
  $cands = New-Object System.Collections.ArrayList
  foreach ($c in @(Get-Command python.exe -CommandType Application -All -ErrorAction SilentlyContinue)) {
    if (-not (Test-StoreAlias $c.Source)) { [void]$cands.Add($c.Source) }
  }
  foreach ($base in @("$env:LOCALAPPDATA\Programs\Python")) {
    foreach ($d in @(Get-ChildItem -LiteralPath $base -Directory -Filter 'Python3*' -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending)) {
      [void]$cands.Add((Join-Path $d.FullName 'python.exe'))
    }
  }
  $py = Find-Exe py
  if ($py) {
    $ErrorActionPreference = 'Continue'
    $exe = @(& $py -3 -c 'import sys; print(sys.executable)' 2>$null) | Select-Object -First 1
    if ($LASTEXITCODE -eq 0 -and $exe) { [void]$cands.Add("$exe".Trim()) }
  }
  foreach ($e in $cands) {
    if (-not (Test-Path -LiteralPath $e)) { continue }
    if (-not (Test-AtLeast (Get-ToolVersion $e) $Min)) { continue }
    if (Test-Writable (Split-Path -Parent $e)) { return $e }
  }
  return $null
}

# The tracked tools/*.sh (and every other *.sh) that are checked out with CRLF.
function Get-CrlfScripts {
  if (-not (Test-Path -LiteralPath (Join-Path $Root '.git'))) { return @() }
  $git = Find-Exe git
  if (-not $git) { return @() }
  $ErrorActionPreference = 'Continue'
  $files = @(& $git -C $Root ls-files -- '*.sh' 2>$null)
  @($files | Where-Object {
      $p = Join-Path $Root $_
      (Test-Path -LiteralPath $p) -and ([IO.File]::ReadAllText($p).Contains("`r"))
    })
}

# Everything missing, broken or too old: why (for the report), the winget packages to ask for,
# the directories to put on PATH, the python.exe to give a python3.exe, and MSVC's fix.
function Invoke-Scan {
  $script:Why = New-Object System.Collections.ArrayList
  $script:Want = New-Object System.Collections.ArrayList
  $script:PathAdd = New-Object System.Collections.ArrayList
  $script:Force = New-Object System.Collections.ArrayList
  $script:Alias = $null
  $script:MsvcFix = ''
  $script:Crlf = @(Get-CrlfScripts)

  $script:Msvc = Find-Msvc
  if (-not $script:Msvc) {
    $script:MsvcFix = 'install'
    [void]$Why.Add("MSVC missing (Visual Studio 2026's Build Tools with the C++ workload)")
  } elseif (-not (Test-WinSdk)) {
    $script:MsvcFix = 'sdk'
    [void]$Why.Add("the Windows SDK missing (MSVC is in $($script:Msvc.installationPath))")
  }

  foreach ($t in $Tools) {
    $exe = Find-Exe $t.Cmd
    $v = Get-ToolVersion $exe $t.VersionArg
    if (Test-ToolOk $v $t) { continue }
    $need = "needs $(if ($t.Exact) { 'exactly ' })$($t.Min)"
    # Installed where its installer puts it, but not on PATH.
    $found = $null
    foreach ($d in $t.Dirs) {
      $e = Join-Path $d "$($t.Cmd).exe"
      if ((Test-Path -LiteralPath $e) -and (Test-ToolOk (Get-ToolVersion $e $t.VersionArg) $t)) { $found = $d; break }
    }
    if ($found -and -not (Test-OnPath $found)) {
      if (-not ($PathAdd -contains $found)) { [void]$PathAdd.Add($found) }
      [void]$Why.Add("$($t.Cmd) is in $found, which is not on PATH")
      continue
    }
    if ($t.Cmd -eq 'python3') {
      $py = Find-Python $t.Min
      if ($py) {
        $script:Alias = $py
        $dir = Split-Path -Parent $py
        if (-not (Test-OnPath $dir) -and -not ($PathAdd -contains $dir)) { [void]$PathAdd.Add($dir) }
        $was = 'missing (only Windows'' Store alias)'
        if ($exe) { $was = "$(if ($v) { $v } else { 'broken' }) at $exe" }
        [void]$Why.Add("python3 $was, but $py is Python $(Get-ToolVersion $py)")
        continue
      }
    }
    $have = 'missing'
    if ($v) { $have = $v } elseif ($exe) {
      $have = "broken (at $exe, fails to run)"
      if (-not ($Force -contains $t.Id)) { [void]$Force.Add($t.Id) }
    }
    if ($found) { $have = "$have at $exe, ahead of $found on PATH" }
    if (-not (@($Want | ForEach-Object { $_.Id }) -contains $t.Id)) { [void]$Want.Add($t) }
    [void]$Why.Add("$($t.Cmd) $have, $need")
  }

  # Git Bash, where tools/*.sh and the Makefile's recipes run.
  $git = Find-Exe git
  $gitRow = $Tools | Where-Object { $_.Cmd -eq 'git' }
  if ($git -and (Test-ToolOk (Get-ToolVersion $git) $gitRow)) {
    $bash = Find-GitBash $git
    $bv = ''
    if ($bash) { $bv = Get-ToolVersion $bash }
    if (-not $bash -or -not (Test-AtLeast $bv '4.0')) {
      if (-not (@($Want | ForEach-Object { $_.Id }) -contains 'Git.Git')) { [void]$Want.Add($gitRow) }
      if ($bash) { [void]$Why.Add("Git Bash $bv at $bash, needs 4.0") }
      else { [void]$Why.Add("Git Bash missing: $git is not Git for Windows (no bin\bash.exe beside it)") }
    }
  }

  if ($script:Crlf.Count) {
    [void]$Why.Add("$($script:Crlf.Count) shell scripts are checked out with CRLF line endings (e.g. $($script:Crlf[0])); bash cannot run them")
  }
}

# What the tree is not tested with, or will trip over, without failing the setup.
function Get-Notes {
  $notes = New-Object System.Collections.ArrayList
  if (-not $X64) {
    [void]$notes.Add("this is $env:PROCESSOR_ARCHITECTURE Windows; only x64 is tested, and Dawn publishes no prebuilt GPU backend for it (configure with -DGE_GPU=OFF)")
  }
  if ($env:CXX -and $env:CXX -notmatch '(^|[\\/])cl(\.exe)?$') {
    [void]$notes.Add("CXX=$env:CXX is not MSVC's cl; CMake builds with it, and only MSVC is tested on Windows; clear CXX and delete build\ to use cl")
  }
  $plain = @($Symlinks | Where-Object {
      $p = Join-Path $Root $_
      (Test-Path -LiteralPath $p) -and -not ((Get-Item -LiteralPath $p -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)
    })
  if ($plain.Count) {
    [void]$notes.Add("the checkout's symlinks ($($plain -join ', ')) are plain files, so make agents-check fails on them; to fix, turn on Developer Mode (Settings > System > For developers), run git config core.symlinks true, delete them and git checkout -- them")
  }
  return $notes
}

function Write-Report {
  foreach ($w in $script:Why) { Write-Host "setup-windows: $w" }
}

function Get-UpToDate {
  "up to date (MSVC in Visual Studio $($script:Msvc.installationVersion), $($Tools.Count) tools)"
}

function Write-Notes {
  foreach ($n in @(Get-Notes)) { Warn "note: $n" }
}

# ---- Changing ------------------------------------------------------------------------------------

# Directories put first in this account's PATH (kept as REG_EXPAND_SZ, as Windows writes it).
function Add-UserPath([string[]]$Dirs) {
  if (-not $Dirs.Count) { return }
  $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Environment', $true)
  $cur = [string]$key.GetValue('Path', '', [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
  $parts = @($cur -split ';' | Where-Object { $_ })
  $have = @($parts | ForEach-Object { [Environment]::ExpandEnvironmentVariables($_).TrimEnd('\') })
  $new = @($Dirs | Where-Object { -not ($have -contains $_.TrimEnd('\')) })
  if ($new.Count) {
    Write-Host "== user PATH: $($new -join ';') first"
    $key.SetValue('Path', ((@($new) + $parts) -join ';'), [Microsoft.Win32.RegistryValueKind]::ExpandString)
  }
  $key.Close()
  if ($new.Count) {
    # Tell Explorer, so terminals opened from now on see it (setting a variable broadcasts that the
    # environment changed).
    [Environment]::SetEnvironmentVariable('GE_SETUP_WINDOWS', '1', 'User')
    [Environment]::SetEnvironmentVariable('GE_SETUP_WINDOWS', $null, 'User')
  }
}

function Invoke-Winget([string[]]$WingetArgs) {
  Write-Host "== winget $($WingetArgs -join ' ')"
  $ErrorActionPreference = 'Continue'
  & winget @WingetArgs
  # winget answers non-zero for "already installed, no newer version" too; the scan after the
  # installs is what decides.
  if ($LASTEXITCODE -ne 0) { Warn "winget exited $LASTEXITCODE for $($WingetArgs[2]) (output above)" }
}

function Install-Package($T) {
  $wa = @('install', '--id', $T.Id, '--exact', '--source', 'winget',
    '--accept-package-agreements', '--accept-source-agreements')
  if ($T.Version) { $wa += @('--version', $T.Version) }
  if ($T.Scope) { $wa += @('--scope', $T.Scope) }
  # A broken install winget calls current is installed again over itself.
  if ($script:Force -contains $T.Id) { $wa += '--force' }
  if ($T.Override) { $wa += @('--override', $T.Override) } else { $wa += '--silent' }
  Invoke-Winget $wa
}

# Visual Studio's own installer, elevated, on an instance already there.
function Edit-VsInstance([string]$InstallPath, [string[]]$Add) {
  $setup = Join-Path $VsInstallerDir 'setup.exe'
  if (-not (Test-Path -LiteralPath $setup)) { Fail "no Visual Studio Installer at $setup; repair Visual Studio, then run tools\setup-windows.ps1 again" }
  $sa = @('modify', '--installPath', "`"$InstallPath`"") + $Add + @('--passive', '--norestart')
  Write-Host "== Visual Studio Installer $($sa -join ' ') (it asks for an administrator)"
  try { $p = Start-Process -FilePath $setup -ArgumentList $sa -Verb RunAs -Wait -PassThru }
  catch { Fail "Visual Studio's installer did not start ($($_.Exception.Message)); it needs an administrator: accept the UAC prompt, then run tools\setup-windows.ps1 again" }
  Test-VsExit $p.ExitCode
}

function Test-VsExit([int]$Code) {
  if ($Code -eq 3010) { $script:Reboot = $true; return }
  if ($Code -ne 0) { Warn "Visual Studio's installer exited $Code" }
}

function Install-Msvc {
  if ($script:MsvcFix -eq 'sdk') {
    Edit-VsInstance $script:Msvc.installationPath @('--add', $VsSdk)
    return
  }
  $bt = Find-BuildTools
  if ($bt) {
    Edit-VsInstance $bt.installationPath $VsWorkload
    return
  }
  $override = (@('--wait', '--passive', '--norestart') + $VsWorkload) -join ' '
  Invoke-Winget @('install', '--id', $VsId, '--exact', '--source', 'winget',
    '--accept-package-agreements', '--accept-source-agreements', '--override', $override)
  # winget reports the bootstrapper's "restart needed" as an error code of its own; a non-zero exit
  # after which MSVC is there is most likely that.
  if ($LASTEXITCODE -ne 0 -and (Find-Msvc)) { $script:Reboot = $true }
}

# Each CRLF script with no change of yours, checked out again with LF (.gitattributes: *.sh eol=lf).
function Repair-Crlf {
  $git = Find-Exe git
  $ErrorActionPreference = 'Continue'
  $kept = New-Object System.Collections.ArrayList
  foreach ($f in $script:Crlf) {
    & $git -C $Root diff --quiet -- $f 2>$null
    if ($LASTEXITCODE -ne 0) { [void]$kept.Add($f); continue }
    Remove-Item -LiteralPath (Join-Path $Root $f) -Force
    & $git -C $Root checkout -- $f
  }
  if ($script:Crlf.Count -gt $kept.Count) {
    Write-Host "== $($script:Crlf.Count - $kept.Count) shell scripts checked out again with LF"
  }
  if ($kept.Count) {
    Warn "these have changes of yours and keep their CRLF; commit or stash them, then run tools\setup-windows.ps1 again: $($kept -join ', ')"
  }
}

# MSVC's x64 environment (vcvars64.bat) brought into this process.
function Enter-MsvcEnv {
  $ErrorActionPreference = 'Continue'
  $bat = Join-Path $script:Msvc.installationPath 'VC\Auxiliary\Build\vcvars64.bat'
  $tmp = Join-Path ([IO.Path]::GetTempPath()) "ge-vcvars-$PID.cmd"
  Set-Content -LiteralPath $tmp -Value "@call `"$bat`" >nul || exit /b 1`r`n@set" -Encoding Ascii
  try { $lines = @(& cmd.exe /d /c $tmp) } finally { Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue }
  if ($LASTEXITCODE -ne 0) { Fail "$bat failed" }
  foreach ($line in $lines) {
    if ($line -match '^([^=]+)=(.*)$') { Set-Item -LiteralPath "env:$($Matches[1])" -Value $Matches[2] }
  }
}

function Invoke-Step([string]$Exe, [string[]]$StepArgs) {
  # A compiler's warnings on stderr are not failures; the exit code is.
  $ErrorActionPreference = 'Continue'
  Write-Host "== $Exe $($StepArgs -join ' ')"
  & $Exe @StepArgs
  if ($LASTEXITCODE -ne 0) { Fail "$Exe $($StepArgs -join ' ') failed (exit $LASTEXITCODE)" }
}

# ---- The run -------------------------------------------------------------------------------------

$Reboot = $false
Invoke-Scan

if ($Mode -eq 'check') {
  if ($Why.Count) {
    Write-Report
    Write-Host 'setup-windows: run tools\setup-windows.ps1 to install them'
    Write-Notes
    exit 1
  }
  Say "setup-windows: $(Get-UpToDate)"
  Write-Notes
  exit 0
}

if (-not $Why.Count) {
  Say "setup-windows: $(Get-UpToDate); nothing to do"
} else {
  Write-Report

  # 1. MSVC, and 2. the winget packages.
  if ($MsvcFix -eq 'install' -or $Want.Count) {
    if (-not (Find-Exe winget)) {
      Fail 'winget missing: install App Installer from the Microsoft Store (https://aka.ms/getwinget), then run tools\setup-windows.ps1 again'
    }
  }
  if ($MsvcFix) { Install-Msvc }
  if (@($Want | Where-Object { $_.Id -eq 'LLVM.LLVM' }).Count) {
    $v = Get-ToolVersion (Join-Path $LlvmDir 'clang-format.exe')
    if ($v -and $v.Split('.')[0] -ne '18') {
      Fail "$PF\LLVM holds LLVM $v, and installing 18 there would replace it; the tree needs clang-format 18 (CI's major). Uninstall it, or put an LLVM 18's bin first on PATH, then run tools\setup-windows.ps1 again"
    }
  }
  foreach ($t in $Want) { Install-Package $t }
  Update-SessionPath

  # 3. PATH and python3, now that the installers are done.
  Invoke-Scan
  Add-UserPath @($PathAdd)
  Update-SessionPath
  if ($Alias) {
    $py3 = Join-Path (Split-Path -Parent $Alias) 'python3.exe'
    Write-Host "== $py3 (a copy of python.exe)"
    Copy-Item -LiteralPath $Alias -Destination $py3 -Force
  }

  # 4. The checkout's shell scripts.
  if ($Crlf.Count) { Repair-Crlf }

  Invoke-Scan
  if ($Why.Count) {
    Write-Report
    foreach ($t in $Tools) {
      $exe = Find-Exe $t.Cmd
      if (Test-ToolOk (Get-ToolVersion $exe $t.VersionArg) $t) { continue }
      if ($exe) {
        Warn "$($t.Cmd) is still $(Get-ToolVersion $exe $t.VersionArg) at $exe; a copy earlier on PATH (the machine's PATH comes before yours) shadows the new one: remove it or reorder PATH"
      }
    }
    if ($Reboot) { Warn 'Visual Studio asked for a restart: restart Windows, then run tools\setup-windows.ps1 again' }
    Fail 'still missing, broken or too old after installing (above)'
  }
  Write-Host "setup-windows: $(Get-UpToDate)"
  if ($Reboot) { Warn 'Visual Studio may need a restart (its installer said so, or winget exited non-zero above): restart Windows before building' }
  $devShell = Join-Path $Msvc.installationPath 'Common7\Tools\Launch-VsDevShell.ps1'
  Write-Host 'setup-windows: open a new terminal so it sees the new PATH. Build in MSVC''s x64 environment:'
  Write-Host "  & '$devShell' -Arch amd64 -HostArch amd64 -SkipAutomaticLocation"
  Write-Host '  cmake --preset release; cmake --build --preset release; ctest --preset release'
  Write-Host '  (or "x64 Native Tools Command Prompt for VS"; start Git Bash from it for make PRESET=release)'
}
Write-Notes

if ($Mode -eq 'verify') {
  Write-Host '== cmake --preset release, then the whole suite on this machine''s GPU (a GPU test that skips fails)'
  Enter-MsvcEnv
  $jobs = $env:NUMBER_OF_PROCESSORS
  if (-not $jobs) { $jobs = '4' }
  Invoke-Step cmake @('--preset', 'release')
  Invoke-Step cmake @('--build', '--preset', 'release')
  $env:GE_GPU_REQUIRE = '1'
  Invoke-Step ctest @('--preset', 'release', '-j', $jobs)
  Write-Host 'setup-windows: verified'
}
