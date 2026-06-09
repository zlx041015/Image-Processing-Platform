# Medical Image Analysis Platform

This repository contains a Qt/C++ desktop application for medical image analysis experiments.

## Main Capabilities

- BMP and common grayscale image loading
- Histogram enhancement
- Noise addition and filtering
- Edge structure analysis
- Image enhancement workflow
- Frequency-domain analysis
- Image restoration, including:
  - atmospheric turbulence degradation/restoration
  - motion blur degradation/restoration
  - inverse filtering
  - cutoff inverse filtering
  - Wiener filtering

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

## Release

The Release build is deployed automatically with Qt runtime files. After building, run:

```text
build/Release/MedicalImageAnalysisPlatform.exe
```

If you want to distribute the app through GitHub, upload the full `build/Release` directory, not only the `.exe`.

## Installer

Installer-related files are in `installer/`.

## Assets

Experiment and sample images are kept in the `图片/` directory.
