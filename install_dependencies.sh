#!/bin/bash
# install_dependencies.sh - Installation des dépendances CBCTAlign
# Usage: sudo ./install_dependencies.sh

set -e

echo "=========================================="
echo "Installation des dépendances CBCTAlign"
echo "=========================================="

# Détecter la distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    DISTRO="unknown"
fi

echo "Distribution détectée: $DISTRO"

install_ubuntu() {
    echo "Installation pour Ubuntu/Debian..."
    
    apt-get update
    
    # Outils de build
    apt-get install -y \
        build-essential \
        cmake \
        git \
        pkg-config
    
    # Qt5 (ou Qt6)
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
    
    # ITK (peut nécessiter compilation manuelle pour la dernière version)
    apt-get install -y libinsighttoolkit5-dev || {
        echo "ITK non disponible via apt, compilation depuis les sources..."
        install_itk_from_source
    }
    
    echo "✓ Dépendances Ubuntu installées"
}

install_itk_from_source() {
    echo "Compilation d'ITK depuis les sources..."
    
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
    
    echo "✓ ITK compilé et installé"
}

install_arch() {
    echo "Installation pour Arch Linux..."
    
    pacman -Syu --noconfirm
    pacman -S --noconfirm \
        base-devel \
        cmake \
        git \
        qt6-base \
        eigen \
        vtk \
        insight-toolkit
    
    echo "✓ Dépendances Arch installées"
}

install_fedora() {
    echo "Installation pour Fedora..."
    
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
    
    echo "✓ Dépendances Fedora installées"
}

# Installation selon la distribution
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
        echo "Distribution non supportée: $DISTRO"
        echo "Veuillez installer manuellement: Qt5/6, VTK, ITK, Eigen3"
        exit 1
        ;;
esac

echo ""
echo "=========================================="
echo "✓ Installation terminée!"
echo "=========================================="
echo ""
echo "Pour compiler CBCTAlign:"
echo "  cd CBCTAlign"
echo "  mkdir build && cd build"
echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
echo "  make -j\$(nproc)"
echo "  ./CBCTAlign"
echo ""
