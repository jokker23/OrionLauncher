set exePath="%cd%\release\launcher.exe"
set targetPath="%cd%\bin"
:: avoid using windeployqt, lets do manually to keep it minimal
:: Qt5Core.dll Qt5Gui.dll Qt5Network.dll Qt5Widget.dll platforms/qwindows.dll styles/qwindowsvistastyle.dll
::%QT_BIN%windeployqt -dir %targetPath% %exePath%
copy %exePath% /Y /B %targetPath%\launcher.exe
pause