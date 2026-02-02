#!/usr/bin/env bash

set -e  # stop on first error

echo "🔧 Installing build dependencies for the project..."

sudo dnf install -y \
  gcc-c++ \
  pkg-config \
  qt5-qtbase-devel \
  qt5-qttools-devel \
  vlc-devel \
  taglib-devel \
  vlc

echo "✅ All dependencies installed successfully!"
echo
echo "You should now be able to compile with:"
echo "g++ main.cpp -std=c++17 -fPIC $(pkg-config --cflags --libs Qt5Widgets) -lvlc -ltag"
