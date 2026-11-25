@echo off
setlocal
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PGJR_VS=%%I"
if not defined PGJR_VS exit /b 1
call "%PGJR_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cmake -S "%~dp0." -B "%~dp0build\fltk" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DPGADM_FLTK_CELL_SOURCE_DIR=%~dp0..\fltk-cell" "-DFLTK_CELL_CSS_SOURCE_DIR=%~dp0..\fltk-css" "-DPGADM_FLTK_DOCK_SOURCE_DIR=%~dp0..\fltk-dock" "-DPGADM_FLTK_TOOLBAR_SOURCE_DIR=%~dp0..\fltk-toolbar" %*
if errorlevel 1 exit /b 1
cmake --build "%~dp0build\fltk" --parallel 4
exit /b %errorlevel%
