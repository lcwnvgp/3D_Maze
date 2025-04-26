@echo off
echo Checking environment...

if "%VULKAN_SDK%"=="" (
    echo Error: VULKAN_SDK environment variable is not set!
    echo Please install Vulkan SDK and set the environment variable.
    pause
    exit /b 1
)

if not exist "%VULKAN_SDK%\Bin\glslangValidator.exe" (
    echo Error: glslangValidator.exe not found in Vulkan SDK!
    echo Please ensure Vulkan SDK is properly installed.
    pause
    exit /b 1
)

echo Creating shaders directory if it doesn't exist...
if not exist "shaders" mkdir shaders

echo Compiling raygen shader...
"%VULKAN_SDK%\Bin\glslangValidator.exe" -V --target-env vulkan1.2 -S rgen shaders/raygen.rgen -o shaders/raygen.rgen.spv
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to compile raygen shader!
    pause
    exit /b 1
)

echo Compiling miss shader...
"%VULKAN_SDK%\Bin\glslangValidator.exe" -V --target-env vulkan1.2 -S rmiss shaders/miss.rmiss -o shaders/miss.rmiss.spv
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to compile miss shader!
    pause
    exit /b 1
)

echo Compiling closesthit shader...
"%VULKAN_SDK%\Bin\glslangValidator.exe" -V --target-env vulkan1.2 -S rchit shaders/closesthit.rchit -o shaders/closesthit.rchit.spv
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to compile closesthit shader!
    pause
    exit /b 1
)


echo Verifying compiled shaders...
if not exist "shaders\raygen.rgen.spv" (
    echo Error: raygen.rgen.spv was not created!
    pause
    exit /b 1
)

if not exist "shaders\miss.rmiss.spv" (
    echo Error: miss.rmiss.spv was not created!
    pause
    exit /b 1
)

if not exist "shaders\closesthit.rchit.spv" (
    echo Error: closesthit.rchit.spv was not created!
    pause
    exit /b 1
)

echo Shader compilation completed successfully!
echo The following files were created:
dir /b shaders\*.spv
pause 