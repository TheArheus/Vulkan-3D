@echo off
if not defined DevEnvDir (
	call vcvarsall x64
)

set VulkanInc="%VULKAN_SDK%\Include"
set VulkanLib="%VULKAN_SDK%\Lib"

set OneFile=/Fe"VulkanEngine" /Fd"VulkanEngine" 
set CommonCompFlags=-nologo -fp:fast -MT -EHa- -Od -Oi -WX- -W4 -GR- -Gm- -GS -FC -Z7 -D_MBCS -DVK_DEBUG=1 -wd4100 -wd4189 -wd4201 -wd4324 -wd4530 -D_CRT_SECURE_NO_WARNINGS -DENABLE_SPIRV_CODEGEN=ON /I%VulkanInc%
set CommonLinkFlags=-opt:ref -incremental:no /SUBSYSTEM:console /LIBPATH:%VulkanLib%

if not exist ..\build\ mkdir ..\build\
pushd ..\build\
C:\DirectXShaderCompiler.bin\Debug\bin\dxc.exe -O2 -spirv -T vs_6_6 -E main ..\shaders\object.vert.hlsl -Fo ..\shaders\object.vert.spv -enable-16bit-types -fspv-target-env=vulkan1.2 -fspv-extension=SPV_KHR_16bit_storage -fspv-extension=SPV_KHR_shader_draw_parameters
C:\DirectXShaderCompiler.bin\Debug\bin\dxc.exe -O2 -spirv -T ms_6_6 -E main ..\shaders\object.mesh.hlsl /Zi -Fo ..\shaders\object.mesh.spv -enable-16bit-types -fspv-target-env=vulkan1.2 -fspv-extension=SPV_NV_mesh_shader -fspv-extension=SPV_KHR_16bit_storage -fspv-extension=SPV_KHR_shader_draw_parameters -DVK_DEBUG=0
C:\DirectXShaderCompiler.bin\Debug\bin\dxc.exe -O2 -spirv -T as_6_6 -E main ..\shaders\object.task.hlsl /Zi -Fo ..\shaders\object.task.spv -enable-16bit-types -fspv-target-env=vulkan1.2 -fspv-extension=SPV_NV_mesh_shader -fspv-extension=SPV_KHR_16bit_storage -fspv-extension=SPV_KHR_shader_draw_parameters
C:\DirectXShaderCompiler.bin\Debug\bin\dxc.exe -O2 -spirv -T cs_6_6 -E main ..\shaders\draw_cull.comp.hlsl /Zi -Fo ..\shaders\draw_cull.comp.spv -enable-16bit-types -fspv-target-env=vulkan1.2 -fspv-extension=SPV_KHR_16bit_storage
C:\DirectXShaderCompiler.bin\Debug\bin\dxc.exe -O2 -spirv -T cs_6_6 -E main ..\shaders\draw_cullate.comp.hlsl /Zi -Fo ..\shaders\draw_cullate.comp.spv -enable-16bit-types -fspv-target-env=vulkan1.2 -fspv-extension=SPV_KHR_16bit_storage
C:\DirectXShaderCompiler.bin\Debug\bin\dxc.exe -O2 -spirv -T cs_6_6 -E main ..\shaders\depth_reduce.comp.hlsl /Zi -Fo ..\shaders\depth_reduce.comp.spv -enable-16bit-types -fspv-target-env=vulkan1.2 -fspv-extension=SPV_KHR_16bit_storage
rem glslangValidator --target-env vulkan1.2 ..\shaders\depth_reduce.comp.glsl -V -o ..\shaders\depth_reduce.comp.spv
C:\DirectXShaderCompiler.bin\Debug\bin\dxc.exe -O2 -spirv -T ps_6_6 -E main ..\shaders\object.frag.hlsl -Fo ..\shaders\object.frag.spv -enable-16bit-types -fspv-target-env=vulkan1.2
cl %CommonCompFlags% user32.lib kernel32.lib vulkan-1.lib ..\code\main.cpp %OneFile% /link %CommonLinkFlags%
popd
