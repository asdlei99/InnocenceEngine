cd ../Res/Shaders/VK
del /S /Q *.spv
for /r %%i in (*) do glslangValidator.exe -V -o %%~i.spv %%i
pause