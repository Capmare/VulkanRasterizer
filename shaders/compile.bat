%VULKAN_SDK%/Bin/glslc.exe %CD%shaders/shader.vert -o vert.spv
%VULKAN_SDK%/Bin/glslc.exe %CD%shaders/shader.frag -o frag.spv
%VULKAN_SDK%/Bin/glslc.exe %CD%shaders/depth.vert -o DepthVert.spv
%VULKAN_SDK%/Bin/glslc.exe %CD%shaders/depth.frag -o DepthFrag.spv
%VULKAN_SDK%/Bin/glslc.exe %CD%shaders/GBuffer.vert -o GBufferVert.spv
%VULKAN_SDK%/Bin/glslc.exe %CD%shaders/GBuffer.frag -o GBufferFrag.spv
pause