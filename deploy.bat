CD /D %~dp0
REM SET PATH=C:\Qt\Qt5.9.8\5.9.8\msvc2015_64\bin;%PATH%
REM windeployqt --plugindir "./release/plugins" --no-translations "./release/BinanceWatcher.exe"

REM COPY /Y .\bin\* .\release\*
REN ".\release" "BinanceWatcher"
"C:\Program Files\WinRAR\WinRAR.exe" a -afzip -m5 -ag_YYYYMMDDHHMMSS ".\BinanceWatcher.zip" ".\BinanceWatcher"
REN ".\BinanceWatcher" "release"
PAUSE