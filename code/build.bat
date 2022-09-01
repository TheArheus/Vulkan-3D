@echo off
if not defined DevEnvDir (
	call vcvarsall x64
)

set VulkanInc="%VULKAN_SDK%\Include"
set VulkanLib="%VULKAN_SDK%\Lib"

set OneFile=/Fe"VulkanEngine" /Fd"VulkanEngine" 
set CommonCompFlags=-nologo -fp:fast -MTd -EHa- -Od -Oi -WX- -W4 -GR- -Gm- -GS -FC -Z7 -D_MBCS -D_DEBUG=1 -wd4100 -D_CRT_SECURE_NO_WARNINGS -DENABLE_SPIRV_CODEGEN=ON /I%VulkanInc%
set CommonLinkFlags=-opt:ref -incremental:no /SUBSYSTEM:console /LIBPATH:%VulkanLib%

if not exist ..\build\ mkdir ..\build\
pushd ..\build\
dxc.exe -spirv -T vs_6_0 -E main ..\shaders\triangle.vert.hlsl -Fo ..\shaders\triangle.vert.spv
dxc.exe -spirv -T ps_6_0 -E main ..\shaders\triangle.frag.hlsl -Fo ..\shaders\triangle.frag.spv
cl %CommonCompFlags% user32.lib kernel32.lib vulkan-1.lib ..\code\main.cpp %OneFile% /link %CommonLinkFlags%
popd
