CD /D %~dp0
SET PATH=C:\Qt\Qt5.9.9\5.9.9\msvc2015_64\bin;%PATH%
windeployqt --plugindir "./release/plugins" --no-translations "./release/BinanceWatcher.exe"

COPY /Y .\bin\* .\release\*
PAUSE