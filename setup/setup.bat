rem call init.bat
call init.bat
rem call clientApp
..\src\bin\Output\ClientApp.exe

if errorlevel 0 goto success
goto failed
:success
rem install dependencies
call npminstall.bat
rem npm install
cd ..\src\lambda
call npminstall.bat
cd ..\..\server
call npminstall.bat
cd ..\setup

rem deploying...
call node setup.js

rem run server...
cd ..\server
start call node server.js

rem load the test page...
cd ..\setup
start "" http://localhost:8080

goto done

:failed
echo setup failed

:done

