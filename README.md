# Medical Image Analysis Platform

Qt/C++ desktop software for medical image loading, enhancement, segmentation, and 3D reconstruction.

## Functional Checklist

This project currently supports the following capabilities:

- Medical-style workspace layout with:
  - native menu bar
  - native toolbar
  - file list / data tree area
  - image display area
  - unified information / parameter panel
- DICOM image opening, parsing, and display:
  - single-file loading
  - directory / series loading
  - axial / coronal / sagittal views
  - window width / window level adjustment
- Medical image segmentation:
  - manual threshold segmentation
  - Otsu automatic threshold segmentation
  - multi-seed region growing
  - DICOM SEG import and aligned overlay display
- Image enhancement:
  - histogram operations
  - noise addition and filtering
  - edge enhancement
  - frequency-domain processing
  - restoration experiments
- 3D reconstruction:
  - asynchronous preview and full-quality surface reconstruction
  - 3D model display with interactive rotation / pan / zoom
  - axial / coronal / sagittal plane positions shown in 3D view

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

## Run

After a successful build, launch:

```text
build/Release/MedicalImageAnalysisPlatform.exe
```

## Suggested Demo Flow

1. Open a DICOM file or a DICOM folder from the File menu or the image-input toolbar section.
2. Verify axial, coronal, and sagittal views update correctly.
3. Adjust window width / window level.
4. Import a DICOM SEG file, or run threshold / Otsu / region-growing segmentation.
5. Observe 2D overlay results and the 3D reconstruction result.
6. Rotate the 3D model and confirm that the three slice planes are visible in 3D.
7. Open enhancement / frequency / restoration functions from the menu bar or toolbar.

## DICOM Support Notes

- Standard DICOM files with `DICM` preamble are supported.
- Files without a `DICM` preamble are now also handled through fallback dataset parsing.
- DICOM SEG import currently assumes little-endian explicit-VR style dataset encoding, which matches the common exported SEG files used in this project.

## Release Packaging

For delivery, package the full `build/Release` directory instead of the `.exe` alone, because Qt runtime files are deployed alongside the application.

## Project Structure

- `src/`: application source code
- `installer/`: installer-related scripts
- `sample image directory (图片/)`: sample experiment images
- `build/Release/`: runnable build output
