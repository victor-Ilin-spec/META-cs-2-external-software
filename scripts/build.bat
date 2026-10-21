@echo off
SETLOCAL EnableDelayedExpansion

set CC=zig c++
set BINARY=cs2-external-esp.exe
set TARGET=x86_64-windows-gnu

set OUTDIR=x64\%TARGET%
set OUT=%OUTDIR%\%BINARY%

if not exist "src\external\lib\libLIBCMT.a" goto :gen_libs
if not exist "src\external\lib\libOLDNAMES.a" goto :gen_libs
goto :skip_gen

:gen_libs
echo generating compat libs
%CC% -c scripts\msvc\compatibility.cpp -o msvc_compat.obj
zig ar rcs src\external\lib\libLIBCMT.a msvc_compat.obj
zig ar rcs src\external\lib\libOLDNAMES.a msvc_compat.obj
del msvc_compat.obj

:skip_gen

set INCLUDES=-Isrc ^
             -Isrc/external ^
             -Isrc/external/imgui ^
             -Isrc/external/json/include ^
             -Isrc/external/curl/include ^
             -Isrc/external/AsyncLogger/src ^
             -Isrc/external/AsyncLogger/include/AsyncLogger

set DEFINES=-D_CONSOLE -DIMGUI_DEFINE_MATH_OPERATORS -DCURL_STATICLIB -D_WIN32_WINNT=0x0a00

:: change -ld3dcompiler_47 to a different version if needed
set LIBS=src/external/lib/libcurl.lib ^
         src/external/lib/zlib.lib ^
         -ld3d11 -ldxgi -ld3dcompiler_47 -ldwmapi -lgdi32 -luser32 -limm32 -lole32 ^
         -lws2_32 -lwldap32 -lcrypt32 -lnormaliz -ladvapi32 -lbcrypt -lwinmm -lshell32

set SRCS=scripts\msvc\compatibility.cpp ^
         src/common.cpp ^
         src/config/Config.cpp ^
         src/core/engine/cache/Cache.cpp ^
         src/core/engine/classes/Bomb.cpp ^
         src/core/engine/classes/Aimbot.cpp ^
         src/core/engine/classes/Triggerbot.cpp ^
         src/core/engine/classes/Strafe.cpp ^
         src/core/engine/classes/Bhop.cpp ^
         src/core/engine/Engine.cpp ^
         src/core/engine/classes/Game.cpp ^
         src/core/engine/classes/Globals.cpp ^
         src/core/engine/classes/Player.cpp ^
         src/core/engine/classes/Weapon.cpp ^
         src/core/engine/classes/ObserverServices.cpp ^
         src/core/engine/vischeck/OptimizedGeometry.cpp ^
         src/core/engine/vischeck/VisCheck.cpp ^
         src/core/engine/vischeck/MapVisibility.cpp ^
         src/core/logger/LogHelper.cpp ^
         src/core/memory/Memory.cpp ^
         src/core/offsets/Dumper.cpp ^
         src/external/AsyncLogger/src/LogCapture.cpp ^
         src/external/AsyncLogger/src/Logger.cpp ^
         src/external/AsyncLogger/src/LogIntermediate.cpp ^
         src/external/AsyncLogger/src/LogMessage.cpp ^
         src/external/AsyncLogger/src/LogStream.cpp ^
         src/external/imgui/backends/imgui_impl_dx11.cpp ^
         src/external/imgui/backends/imgui_impl_win32.cpp ^
         src/external/imgui/imgui.cpp ^
         src/external/imgui/imgui_demo.cpp ^
         src/external/imgui/imgui_draw.cpp ^
         src/external/imgui/imgui_tables.cpp ^
         src/external/imgui/imgui_widgets.cpp ^
         src/external/timer/timer.cpp ^
         src/gui/frontend/esp/Esp.cpp ^
         src/gui/frontend/menu/Menu.cpp ^
         src/gui/newmenu/framework/menu.cc ^
         src/gui/newmenu/framework/gui/window/window.cc ^
         src/gui/newmenu/framework/gui/tab/tab.cc ^
         src/gui/newmenu/framework/gui/child/child.cc ^
         src/gui/newmenu/framework/gui/checkbox/checkbox.cc ^
         src/gui/newmenu/framework/gui/slider/slider.cc ^
         src/gui/newmenu/framework/gui/dropdown/dropdown.cc ^
         src/gui/newmenu/framework/gui/multibox/multibox.cc ^
         src/gui/newmenu/framework/gui/button/button.cc ^
         src/gui/newmenu/framework/gui/colorpicker/colorpicker.cc ^
         src/gui/newmenu/framework/gui/keybind/keybind.cc ^
         src/gui/newmenu/framework/gui/textinput/textinput.cc ^
         src/gui/newmenu/framework/gui/listbox/listbox.cc ^
         src/gui/newmenu/framework/gui/popup/popup.cc ^
         src/gui/newmenu/render/render.cc ^
         src/gui/newmenu/render/fonts/sf_pro_bold.cc ^
         src/gui/newmenu/render/fonts/fa.cc ^
         src/gui/frontend/overlays/Overlays.cpp ^
         src/gui/renderer/Renderer.cpp ^
         src/gui/renderer/window/Window.cpp ^
         src/main.cpp ^
         src/updater/http/HttpHelper.cpp ^
         src/updater/Updater.cpp

mkdir "%OUTDIR%" 2>nul

echo compiling project
%CC% -target %TARGET% -std=c++20 -O3 -Wno-date-time -fno-autolink -include src/common.hpp %DEFINES% %INCLUDES% %SRCS% -o %OUT% %LIBS% -Lsrc/external/lib

if %ERRORLEVEL% EQU 0 (
    echo build successful: %OUT%
) else (
    echo build failed.
)

pause