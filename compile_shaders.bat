@echo off
echo Checking Vulkan environment...

:: Check Vulkan SDK environment variable
if "%VULKAN_SDK%"=="" (
    echo Error: VULKAN_SDK environment variable is not set!
    echo Please install Vulkan SDK and set the environment variable.
    pause
    exit /b 1
)

:: Check for glslc.exe existence
if not exist "%VULKAN_SDK%\Bin\glslc.exe" (
    echo Error: glslc.exe not found in Vulkan SDK!
    echo Please ensure Vulkan SDK is properly installed.
    pause
    exit /b 1
)

:: Ensure shaders directory exists
echo Creating shaders directory if it doesn't exist...
if not exist "shaders" mkdir shaders

:: Compile main vertex shader
echo Compiling vertex shader...
"%VULKAN_SDK%\Bin\glslc.exe" shaders/shader.vert -o shaders/shader.vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to compile vertex shader!
    pause
    exit /b 1
)

:: Compile main fragment shader
echo Compiling fragment shader...
"%VULKAN_SDK%\Bin\glslc.exe" shaders/shader.frag -o shaders/shader.frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to compile fragment shader!
    pause
    exit /b 1
)

:: Verify all compiled SPIR-V files exist
echo Verifying compiled shaders...
if not exist "shaders\shader.vert.spv" (
    echo Error: shader.vert.spv was not created!
    pause
    exit /b 1
)

if not exist "shaders\shader.frag.spv" (
    echo Error: shader.frag.spv was not created!
    pause
    exit /b 1
)

:: Success output
echo Shader compilation completed successfully!
echo The following SPIR-V files were created:
dir /b shaders\*.spv
pause