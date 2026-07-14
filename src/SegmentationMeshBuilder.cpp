#include "SegmentationMeshBuilder.h"

#include <vtkCellArray.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkMarchingCubes.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>

#include <algorithm>

namespace {
QVector<QImage> downsampleMasks(const QVector<QImage>& masks, int step) {
    if (step <= 1 || masks.isEmpty()) {
        return masks;
    }

    const int depth = masks.size();
    const int srcHeight = masks.first().height();
    const int srcWidth = masks.first().width();
    const int outDepth = std::max(1, (depth + step - 1) / step);
    const int outHeight = std::max(1, (srcHeight + step - 1) / step);
    const int outWidth = std::max(1, (srcWidth + step - 1) / step);

    QVector<QImage> reduced;
    reduced.reserve(outDepth);
    for (int oz = 0; oz < outDepth; ++oz) {
        QImage slice(outWidth, outHeight, QImage::Format_Grayscale8);
        slice.fill(0);
        for (int oy = 0; oy < outHeight; ++oy) {
            uchar* dst = slice.scanLine(oy);
            for (int ox = 0; ox < outWidth; ++ox) {
                bool filled = false;
                for (int dz = 0; dz < step && !filled; ++dz) {
                    const int z = oz * step + dz;
                    if (z >= depth) {
                        break;
                    }
                    const QImage gray = masks[z].convertToFormat(QImage::Format_Grayscale8);
                    for (int dy = 0; dy < step && !filled; ++dy) {
                        const int y = oy * step + dy;
                        if (y >= srcHeight) {
                            break;
                        }
                        const uchar* row = gray.constScanLine(y);
                        for (int dx = 0; dx < step; ++dx) {
                            const int x = ox * step + dx;
                            if (x >= srcWidth) {
                                break;
                            }
                            if (row[x] > 0) {
                                filled = true;
                                break;
                            }
                        }
                    }
                }
                dst[ox] = filled ? 255 : 0;
            }
        }
        reduced.append(slice);
    }
    return reduced;
}

vtkSmartPointer<vtkImageData> buildVtkMaskVolume(
    const QVector<QImage>& maskSlices,
    double spacingX,
    double spacingY,
    double spacingZ
) {
    if (maskSlices.isEmpty()) {
        return nullptr;
    }

    const int depth = maskSlices.size();
    const int height = maskSlices.first().height();
    const int width = maskSlices.first().width();

    vtkNew<vtkImageData> image;
    image->SetDimensions(width, height, depth);
    image->SetSpacing(spacingX, spacingY, spacingZ);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);

    for (int z = 0; z < depth; ++z) {
        const QImage gray = maskSlices[z].convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < height; ++y) {
            const uchar* row = gray.constScanLine(y);
            for (int x = 0; x < width; ++x) {
                auto* voxel = static_cast<unsigned char*>(image->GetScalarPointer(x, y, z));
                voxel[0] = row[x] > 0 ? 255 : 0;
            }
        }
    }

    return image;
}

SegmentationSurfaceData polyDataToSurfaceData(vtkPolyData* polyData) {
    SegmentationSurfaceData data;
    if (!polyData || polyData->GetNumberOfPoints() <= 0) {
        return data;
    }

    auto* points = polyData->GetPoints();
    auto* normals = polyData->GetPointData() ? polyData->GetPointData()->GetNormals() : nullptr;
    if (!points) {
        return data;
    }

    data.vertices.reserve(static_cast<int>(points->GetNumberOfPoints()));
    data.normals.reserve(static_cast<int>(points->GetNumberOfPoints()));
    for (vtkIdType i = 0; i < points->GetNumberOfPoints(); ++i) {
        double p[3] = {0.0, 0.0, 0.0};
        points->GetPoint(i, p);
        data.vertices.append(QVector3D(
            static_cast<float>(p[0]),
            static_cast<float>(p[1]),
            static_cast<float>(p[2])
        ));

        if (normals && i < normals->GetNumberOfTuples()) {
            double n[3] = {0.0, 0.0, 1.0};
            normals->GetTuple(i, n);
            data.normals.append(QVector3D(
                static_cast<float>(n[0]),
                static_cast<float>(n[1]),
                static_cast<float>(n[2])
            ));
        } else {
            data.normals.append(QVector3D(0.0f, 0.0f, 1.0f));
        }
    }

    auto* polys = polyData->GetPolys();
    if (!polys) {
        return data;
    }

    polys->InitTraversal();
    vtkIdType pointCount = 0;
    const vtkIdType* pointIds = nullptr;
    while (polys->GetNextCell(pointCount, pointIds)) {
        if (pointCount < 3) {
            continue;
        }
        for (vtkIdType i = 1; i + 1 < pointCount; ++i) {
            data.indices.append(static_cast<int>(pointIds[0]));
            data.indices.append(static_cast<int>(pointIds[i]));
            data.indices.append(static_cast<int>(pointIds[i + 1]));
        }
    }

    return data;
}
}

SegmentationSurfaceData SegmentationMeshBuilder::buildSurfaceMesh(
    const QVector<QImage>& maskSlices,
    double spacingX,
    double spacingY,
    double spacingZ,
    int samplingStep
) {
    samplingStep = std::max(1, samplingStep);
    const QVector<QImage> workingMasks = samplingStep > 1 ? downsampleMasks(maskSlices, samplingStep) : maskSlices;
    const double effectiveSpacingX = spacingX * samplingStep;
    const double effectiveSpacingY = spacingY * samplingStep;
    const double effectiveSpacingZ = spacingZ * samplingStep;

    SegmentationSurfaceData data;
    if (workingMasks.isEmpty()) {
        return data;
    }

    const int depth = workingMasks.size();
    const int height = workingMasks.first().height();
    const int width = workingMasks.first().width();
    data.volumeSize = QVector3D(
        static_cast<float>(width * effectiveSpacingX),
        static_cast<float>(height * effectiveSpacingY),
        static_cast<float>(depth * effectiveSpacingZ)
    );

    vtkSmartPointer<vtkImageData> imageData = buildVtkMaskVolume(
        workingMasks,
        effectiveSpacingX,
        effectiveSpacingY,
        effectiveSpacingZ
    );
    if (!imageData) {
        return data;
    }

    vtkNew<vtkMarchingCubes> marchingCubes;
    marchingCubes->SetInputData(imageData);
    marchingCubes->SetValue(0, 127.5);
    marchingCubes->ComputeNormalsOff();
    marchingCubes->ComputeGradientsOff();
    marchingCubes->Update();

    vtkNew<vtkPolyDataNormals> normalFilter;
    normalFilter->SetInputConnection(marchingCubes->GetOutputPort());
    normalFilter->SplittingOff();
    normalFilter->ConsistencyOn();
    normalFilter->AutoOrientNormalsOn();
    normalFilter->ComputePointNormalsOn();
    normalFilter->ComputeCellNormalsOff();
    normalFilter->Update();

    SegmentationSurfaceData vtkSurface = polyDataToSurfaceData(normalFilter->GetOutput());
    if (!vtkSurface.isEmpty()) {
        vtkSurface.volumeSize = data.volumeSize;
    }
    return vtkSurface;
}
