#!/bin/bash

# EduSync Build Script
# This script builds the EduSync calendar application

echo "🎓 EduSync - AI-Powered Calendar Application"
echo "=============================================="

# Check if Qt is available
if ! command -v qmake &> /dev/null; then
    echo "❌ Qt not found. Please install Qt 6 and ensure qmake is in your PATH."
    echo "   You can download Qt from: https://www.qt.io/download"
    exit 1
fi

# Check if CMake is available
if ! command -v cmake &> /dev/null; then
    echo "❌ CMake not found. Please install CMake 3.16 or higher."
    exit 1
fi

echo "✅ Qt and CMake found"

# Create build directory
echo "📁 Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "⚙️  Configuring project with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed"
    exit 1
fi

# Build the project
echo "🔨 Building project..."
cmake --build . --config Release

if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi

echo "✅ Build successful!"
echo ""
echo "🚀 To run the application:"
echo "   ./EduSync"
echo ""
echo "📱 Or from the project root:"
echo "   cd build && ./EduSync"
echo ""
echo "🎉 Enjoy your AI-powered calendar!"
