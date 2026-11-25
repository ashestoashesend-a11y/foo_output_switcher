# Build foo_output_switcher (x64 Release).
#
# Same environment quirks as foo_openlyrics (see skill: windows-cpp-build):
#  * Only the VS2022 Build Tools are installed here, not a full VS.
#  * The project asks for Windows SDK 10.0.19041 by default; only 10.0.26100 is
#    installed -> override WindowsTargetPlatformVersion.
#  * Building a .vcxproj directly means $(SolutionDir) is empty; this project
#    does not use it (paths come from $(Fb2kSdk) and $(MSBuildThisFileDirectory)).
#  * /nodeReuse:false so no MSBuild worker processes are left behind.
$ErrorActionPreference = 'Stop'

$repo = 'C:\Users\a9949\.hermes\workspace\repos\foo_output_switcher'
$vs = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'

$platform = if ($args.Count -ge 1) { $args[0] } else { 'x64' }
$config = if ($args.Count -ge 2) { $args[1] } else { 'Release' }
$sdk = '10.0.26100.0'

# The SDK's own projects (pfc/libPPUI/helpers/SDK/component_client) locate WTL
# through $(SolutionDir)\..\3rdparty\..., so SolutionDir has to point at the
# foo_openlyrics build directory for those ProjectReferences to compile. This
# project itself never uses $(SolutionDir) - its paths hang off
# $(MSBuildThisFileDirectory).
$solutionDir = 'C:\Users\a9949\.hermes\workspace\repos\foo_openlyrics\build\'

Write-Host "==> foo_output_switcher ($platform $config)"
& $msbuild '/m' '/nodeReuse:false' '/nologo' '/v:minimal' `
    "/p:Configuration=$config" "/p:Platform=$platform" `
    "/p:WindowsTargetPlatformVersion=$sdk" `
    "/p:SolutionDir=$solutionDir" `
    (Join-Path $repo 'build\foo_output_switcher.vcxproj') 2>&1 | Out-Host
$code = $LASTEXITCODE

$dll = Join-Path $repo "build\$platform\$config\foo_output_switcher.dll"
if ($code -eq 0 -and (Test-Path $dll)) {
    $item = Get-Item $dll
    Write-Host ("BUILD OK -> {0}  ({1:N0} bytes, {2})" -f $item.FullName, $item.Length, $item.LastWriteTime)
} else {
    Write-Host "BUILD FAILED (msbuild exit $code)"
}
exit $code
