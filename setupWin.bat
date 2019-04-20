git submodule update

cd source/external/gitsubmodules
xcopy /e/s/y assimp\include\assimp\* ..\include\assimp\

xcopy /y Vulkan-Headers\include\vulkan\* ..\include\vulkan\

xcopy /y glfw\include\GLFW\* ..\include\GLFW\

xcopy /y stb\stb_image.h ..\include\stb\

xcopy /y imgui\*.h ..\..\engine\third-party\ImGui\
xcopy /y imgui\*.cpp ..\..\engine\third-party\ImGui\

xcopy /y imgui\examples\imgui_impl_win32.h ..\..\engine\third-party\ImGui\
xcopy /y imgui\examples\imgui_impl_win32.cpp ..\..\engine\third-party\ImGui\

xcopy /y imgui\examples\imgui_impl_dx11.h ..\..\engine\third-party\ImGui\
xcopy /y imgui\examples\imgui_impl_dx11.cpp ..\..\engine\third-party\ImGui\


xcopy /y imgui\examples\imgui_impl_opengl3.h ..\..\engine\third-party\ImGui\
xcopy /y imgui\examples\imgui_impl_opengl3.cpp ..\..\engine\third-party\ImGui\

xcopy /y imgui\examples\imgui_impl_glfw.h ..\..\engine\third-party\ImGui\
xcopy /y imgui\examples\imgui_impl_glfw.cpp ..\..\engine\third-party\ImGui\

(echo #define IMGUI_IMPL_OPENGL_LOADER_GLAD) >temp.h.new
type ..\..\engine\third-party\ImGui\imgui_impl_opengl3.h >>temp.h.new
move /y temp.h.new ..\..\engine\third-party\ImGui\imgui_impl_opengl3.h

xcopy /y json\single_include\nlohmann\json.hpp ..\include\json\

xcopy /e/s/y Qt-Frameless-Window-DarkStyle\darkstyle\* ..\..\editor\InnocenceEditor\darkstyle\
xcopy /y Qt-Frameless-Window-DarkStyle\darkstyle.qrc ..\..\editor\InnocenceEditor\
xcopy /y Qt-Frameless-Window-DarkStyle\Darkstyle.h ..\..\editor\InnocenceEditor\
xcopy /y Qt-Frameless-Window-DarkStyle\Darkstyle.cpp ..\..\editor\InnocenceEditor\

mkdir ..\include\GL
powershell -Command "Invoke-WebRequest https://www.khronos.org/registry/OpenGL/api/GL/wglext.h -OutFile ..\include\GL\wglext.h"
powershell -Command "Invoke-WebRequest https://www.khronos.org/registry/OpenGL/api/GL/glext.h -OutFile ..\include\GL\glext.h"
mkdir ..\include\DX12
powershell -Command "Invoke-WebRequest https://raw.githubusercontent.com/Microsoft/DirectX-Graphics-Samples/master/Libraries/D3DX12/d3dx12.h -OutFile ..\include\DX12\d3dx12.h"

cd ../

mkdir dll\win
mkdir lib\win

pause
