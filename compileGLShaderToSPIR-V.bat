cd res/shaders/GL
del /S /Q *.spv
for /r %%i in (*) do glslangValidator.exe -G -o %%~i.spv %%i
pause