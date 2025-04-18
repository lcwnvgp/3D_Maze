@echo off
echo Checking environment...

if "%VULKAN_SDK%"=="" (
    echo Error: VULKAN_SDK environment variable is not set!
    echo Please install Vulkan SDK and set the environment variable.
    pause
    exit /b 1
)

if not exist "%VULKAN_SDK%\Bin\glslc.exe" (
    echo Error: glslc.exe not found in Vulkan SDK!
    echo Please ensure Vulkan SDK is properly installed.
    pause
    exit /b 1
)

echo Creating shaders directory if it doesn't exist...
if not exist "shaders" mkdir shaders

echo Compiling vertex shader...
"%VULKAN_SDK%\Bin\glslc.exe" shaders/shader.vert -o shaders/vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to compile vertex shader!
    pause
    exit /b 1
)

echo Compiling fragment shader...
"%VULKAN_SDK%\Bin\glslc.exe" shaders/shader.frag -o shaders/frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to compile fragment shader!
    pause
    exit /b 1
)

echo Verifying compiled shaders...
if not exist "shaders\vert.spv" (
    echo Error: vert.spv was not created!
    pause
    exit /b 1
)

if not exist "shaders\frag.spv" (
    echo Error: frag.spv was not created!
    pause
    exit /b 1
)

echo Shader compilation completed successfully!
echo The following files were created:
dir /b shaders\*.spv
pause 