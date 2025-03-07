#!/bin/bash

# Exit on error
set -e

# Build Windows API Docker image
echo "Building Docker image..."
docker build -t win32-builder -f Dockerfile.win32 .

# Compile in Docker container
echo "Compiling Windows executable in Docker container..."
docker run --rm -v $(pwd):/workspace win32-builder bash -c "
    # Compile application - add UNICODE and _UNICODE definitions
    x86_64-w64-mingw32-gcc -o OneDriveSyncHelper.exe main.c -lcomctl32 -lshell32 -lole32 -luuid -mwindows -DUNICODE -D_UNICODE

    # Check if compilation was successful
    if [ -f 'OneDriveSyncHelper.exe' ]; then
        echo 'Compilation successful!'
    else
        echo 'Compilation failed!'
        exit 1
    fi
"

# Check if executable was generated successfully
if [ -f "OneDriveSyncHelper.exe" ]; then
    echo "================================================="
    echo "Build completed successfully!"
    echo "Windows executable generated: OneDriveSyncHelper.exe"
    echo "================================================="
else
    echo "Build failed! Executable not found."
    exit 1
fi