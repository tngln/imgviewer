#pragma once

#include <d2d1_1.h>
#include <d2d1helper.h>

namespace imgviewer_edit_geometry {

inline int NormalizeEditRotation(int quadrants)
{
    int normalized = quadrants % 4;
    if (normalized < 0) {
        normalized += 4;
    }
    return normalized;
}

inline D2D1_SIZE_U EditPreviewSize(D2D1_SIZE_U source_size, int rotation_quadrants)
{
    const int rotation = NormalizeEditRotation(rotation_quadrants);
    if (rotation % 2 == 0) {
        return source_size;
    }
    return D2D1::SizeU(source_size.height, source_size.width);
}

inline D2D1_POINT_2F SourcePointToEditPreviewPoint(
    D2D1_POINT_2F point,
    D2D1_SIZE_U source_size,
    int rotation_quadrants)
{
    const float source_width = static_cast<float>(source_size.width);
    const float source_height = static_cast<float>(source_size.height);
    switch (NormalizeEditRotation(rotation_quadrants)) {
    case 1:
        return D2D1::Point2F(source_height - point.y, point.x);
    case 2:
        return D2D1::Point2F(source_width - point.x, source_height - point.y);
    case 3:
        return D2D1::Point2F(point.y, source_width - point.x);
    default:
        return point;
    }
}

inline D2D1_POINT_2F EditPreviewPointToSourcePoint(
    D2D1_POINT_2F point,
    D2D1_SIZE_U source_size,
    int rotation_quadrants)
{
    const float source_width = static_cast<float>(source_size.width);
    const float source_height = static_cast<float>(source_size.height);
    switch (NormalizeEditRotation(rotation_quadrants)) {
    case 1:
        return D2D1::Point2F(point.y, source_height - point.x);
    case 2:
        return D2D1::Point2F(source_width - point.x, source_height - point.y);
    case 3:
        return D2D1::Point2F(source_width - point.y, point.x);
    default:
        return point;
    }
}

inline D2D1_MATRIX_3X2_F SourceToEditPreviewTransform(D2D1_SIZE_U source_size, int rotation_quadrants)
{
    const float source_width = static_cast<float>(source_size.width);
    const float source_height = static_cast<float>(source_size.height);
    switch (NormalizeEditRotation(rotation_quadrants)) {
    case 1:
        return D2D1::Matrix3x2F::Rotation(90.0f) *
            D2D1::Matrix3x2F::Translation(source_height, 0.0f);
    case 2:
        return D2D1::Matrix3x2F::Rotation(180.0f) *
            D2D1::Matrix3x2F::Translation(source_width, source_height);
    case 3:
        return D2D1::Matrix3x2F::Rotation(270.0f) *
            D2D1::Matrix3x2F::Translation(0.0f, source_width);
    default:
        return D2D1::Matrix3x2F::Identity();
    }
}

} // namespace imgviewer_edit_geometry
