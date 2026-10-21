@echo off
setlocal
set ROOT=%~dp0..
set EXE=%ROOT%\tools\cs2-phys-extractor\bin\Release\net9.0\PhysExtractor.exe
set OUT=%ROOT%\maps\phys
if not exist "%EXE%" (
  echo Building extractor...
  dotnet build -c Release "%ROOT%\tools\cs2-phys-extractor\PhysExtractor.csproj" || exit /b 1
)
echo Extracting official map .vphys into %OUT%
"%EXE%" --batch --out "%OUT%"
if exist "%ROOT%\x64\Debug\cs2-external-esp.exe" (
  mkdir "%ROOT%\x64\Debug\maps\phys" 2>nul
  mkdir "%ROOT%\x64\Debug\maps\surfaces" 2>nul
  copy /Y "%OUT%\*.vphys" "%ROOT%\x64\Debug\maps\phys\" >nul
  copy /Y "%ROOT%\maps\surfaces\*" "%ROOT%\x64\Debug\maps\surfaces\" >nul
)
echo done
endlocal
