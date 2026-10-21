@echo off
setlocal
set ROOT=%~dp0..
set EXE=%ROOT%\tools\cs2-phys-extractor\bin\Release\net9.0\PhysExtractor.exe
set OUT=%ROOT%\maps
if not exist "%EXE%" (
  echo Building extractor...
  dotnet build -c Release "%ROOT%\tools\cs2-phys-extractor\PhysExtractor.csproj" || exit /b 1
)
mkdir "%OUT%" 2>nul
echo Extracting official maps to lean .opt in %OUT%
"%EXE%" --batch --opt --out "%OUT%"
if exist "%ROOT%\x64\Debug\cs2-external-esp.exe" (
  mkdir "%ROOT%\x64\Debug\maps" 2>nul
  copy /Y "%OUT%\*.opt" "%ROOT%\x64\Debug\maps\" >nul
)
if exist "%ROOT%\x64\Release\cs2-external-esp.exe" (
  mkdir "%ROOT%\x64\Release\maps" 2>nul
  copy /Y "%OUT%\*.opt" "%ROOT%\x64\Release\maps\" >nul
)
echo done
endlocal
