%VULKAN_SDK%/Bin/glslc.exe shader.vert -o vert.spv
%VULKAN_SDK%/Bin/glslc.exe shader.frag -o frag.spv
%VULKAN_SDK%/Bin/glslc.exe depth.vert -o DepthVert.spv
%VULKAN_SDK%/Bin/glslc.exe depth.frag -o DepthFrag.spv
%VULKAN_SDK%/Bin/glslc.exe GBuffer.vert -o GBufferVert.spv
%VULKAN_SDK%/Bin/glslc.exe GBuffer.frag -o GBufferFrag.spv
pause