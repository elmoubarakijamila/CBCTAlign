#!/bin/bash
# install_dependencies.sh - Install CBCTAlign dependencies
# Usage: sudo ./install_dependencies.sh

set -e

echo "=========================================="
echo "Installing CBCTAlign dependencies"
echo "=========================================="

# Detect the distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    DISTRO="unknown"
fi

echo "Detected distribution: $DISTRO"

install_ubuntu() {
    echo "Installing for Ubuntu/Debian..."

    apt-get update

    # Build tools
    apt-get install -y \
        build-essential \
        cmake \
        git \
        pkg-config

    # Qt5 (or Qt6)
    apt-get install -y \
        qtbase5-dev \
        qttools5-dev \
        libqt5opengl5-dev \
        qt5-qmake

    # Eigen3
    apt-get install -y libeigen3-dev

    # VTK
    apt-get install -y \
        libvtk9-dev \
        libvtk9-qt-dev \
        vtk9

    # ITK (may require manual compilation for the latest version)
    apt-get install -y libinsighttoolkit5-dev || {
        echo "ITK not available via apt, building from source..."
        install_itk_from_source
    }

    echo "Ubuntu dependencies installed"
}

install_itk_from_source() {
    echo "Building ITK from source..."

    cd /tmp
    git clone --depth 1 --branch v5.3.0 https://github.com/InsightSoftwareConsortium/ITK.git
    mkdir ITK-build && cd ITK-build

    cmake ../ITK \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DBUILD_TESTING=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DITK_BUILD_DEFAULT_MODULES=ON \
        -DModule_ITKReview=ON

    make -j$(nproc)
    make install

    cd /tmp
    rm -rf ITK ITK-build

    echo "ITK built and installed"
}

install_arch() {
    echo "Installing for Arch Linux..."

    pacman -Syu --noconfirm
    pacman -S --noconfirm \
        base-devel \
        cmake \
        git \
        qt6-base \
        eigen \
        vtk \
        insight-toolkit

    echo "Arch dependencies installed"
}

install_fedora() {
    echo "Installing for Fedora..."

    dnf update -y
    dnf install -y \
        cmake \
        gcc-c++ \
        git \
        qt5-qtbase-devel \
        qt5-qttools-devel \
        eigen3-devel \
        vtk-devel \
        vtk-qt \
        InsightToolkit-devel

    echo "Fedora dependencies installed"
}

# Install based on the distribution
case $DISTRO in
    ubuntu|debian|linuxmint)
        install_ubuntu
        ;;
    arch|manjaro)
        install_arch
        ;;
    fedora)
        install_fedora
        ;;
    *)
        echo "Unsupported distribution: $DISTRO"
        echo "Please install manually: Qt5/6, VTK, ITK, Eigen3"
        exit 1
        ;;
esac

echo ""
echo "=========================================="
echo "Installation complete!"
echo "=========================================="
echo ""
echo "To build CBCTAlign:"
echo "  cd CBCTAlign"
echo "  mkdir build && cd build"
echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
echo "  make -j\$(nproc)"
echo "  ./CBCTAlign"
echo ""
