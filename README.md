# CBCTAlign

**CBCTAlign: Automated Spatio-Temporal Alignment
of Longitudinal CBCT Scans for Bone Regeneration
Assessment**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Qt 5.15](https://img.shields.io/badge/Qt-5.15-green.svg)](https://www.qt.io/)
[![ITK 5.2](https://img.shields.io/badge/ITK-5.2-orange.svg)](https://itk.org/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg)]()

CBCTAlign is an open-source C++/Qt software for the spatio-temporal alignment of longitudinal cone-beam computed tomography (CBCT) volumes, designed for quantitative follow-up of maxillofacial bone regeneration after implant placement, sinus lifting, alveolar grafting, or Guided Bone Regeneration (GBR) procedures.

---

##  Key Features

- **Landmark-initialized rigid registration** — uses cephalometric landmarks (ANS, Sella, Nasion, PNS) from [ALI_CBCT](https://github.com/lucanchling/ALI_CBCT) JSON markup files to initialize Mattes Mutual Information optimization, preventing convergence to local minima.
- **Multi-resolution 3D rigid registration** — ITK-based pipeline with a 3-level pyramid (shrink factors 4, 2, 1).
- **Automatic FOV normalization** — handles heterogeneous CBCT acquisition protocols (large craniofacial FOV vs. reduced dental FOV).
- **Anatomically guided 2D slice extraction** — axial, coronal, and sagittal slices centered on a user-selected reference landmark.
- **MCAGPC composite validation metric** — combines SSIM, normalized cross-correlation (NCC), and post-registration landmark displacement.
- **Synchronized multi-timepoint viewer** — interactive 3×N grid view (3 orientations × N timepoints) for longitudinal inspection.
- **Structured PNG export** — ready-to-use 2D slice series for downstream deep learning pipelines.

---

##  Dependencies

| Library  | Version | Role |
|----------|---------|------|
| ITK      | 5.2     | Image registration, resampling, Mattes MI metric, multi-resolution pyramid |
| Qt       | 5.15    | Graphical user interface (Widgets, Core, GUI modules) |
| VTK      | 9.0     | 3D volume visualization (optional) |
| DCMTK    | 3.6     | DICOM file I/O |
| Eigen    | 3.4     | Linear algebra for transformation matrices |
| CMake    | ≥ 3.16  | Cross-platform build system |

**Compilers tested:** GCC 9+ (Linux), MSVC 2019+ (Windows)
**Operating systems tested:** Ubuntu 20.04+, Ubuntu 24.04, Windows 10+

---

##  Installation

### Linux (Ubuntu 20.04+)

```bash
# 1. Install system dependencies
git clone https://github.com/elmoubarakijamila/CBCTAlign.git
cd CBCTAlign
chmod +x install_dependencies.sh
./install_dependencies.sh

# 2. Build with CMake
mkdir build && cd build
cmake ..
make -j$(nproc)

# 3. Run
./CBCTAlign
```

### Windows 10+

```bash
# Requires Qt 5.15, ITK 5.2, VTK 9.0, DCMTK 3.6, CMake 3.16+ installed via vcpkg or manually
git clone https://github.com/elmoubarakijamila/CBCTAlign.git
cd CBCTAlign
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
```

