cd tango

cmake -Bbuild -H. -GNinja -DCMAKE_INSTALL_PREFIX=%LIBRARY_PREFIX% -DCMAKE_FIND_ROOT_PATH=%LIBRARY_PREFIX%
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cmake --build build --config Release --target install
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
