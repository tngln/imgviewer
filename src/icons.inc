namespace icons {

enum class PathVerb {
    MoveTo,
    LineTo,
    CubicTo,
    Close,
};

struct PathCommand {
    PathVerb verb;
    D2D1_POINT_2F points[3];
};

constexpr float kImageIconViewport = 24.0f;
constexpr float kToolbarIconViewport = 24.0f;

constexpr PathCommand kImageIconPath[] = {
    {PathVerb::MoveTo, {{3.5f, 5.0f}, {}, {}}},
    {PathVerb::LineTo, {{20.5f, 5.0f}, {}, {}}},
    {PathVerb::LineTo, {{20.5f, 19.0f}, {}, {}}},
    {PathVerb::LineTo, {{3.5f, 19.0f}, {}, {}}},
    {PathVerb::Close, {{}, {}, {}}},

    {PathVerb::MoveTo, {{5.0f, 16.5f}, {}, {}}},
    {PathVerb::LineTo, {{10.0f, 11.5f}, {}, {}}},
    {PathVerb::LineTo, {{13.5f, 15.0f}, {}, {}}},
    {PathVerb::LineTo, {{15.5f, 13.0f}, {}, {}}},
    {PathVerb::LineTo, {{19.0f, 16.5f}, {}, {}}},

    {PathVerb::MoveTo, {{16.0f, 8.0f}, {}, {}}},
    {PathVerb::CubicTo, {{17.1046f, 8.0f}, {18.0f, 8.8954f}, {18.0f, 10.0f}}},
    {PathVerb::CubicTo, {{18.0f, 11.1046f}, {17.1046f, 12.0f}, {16.0f, 12.0f}}},
    {PathVerb::CubicTo, {{14.8954f, 12.0f}, {14.0f, 11.1046f}, {14.0f, 10.0f}}},
    {PathVerb::CubicTo, {{14.0f, 8.8954f}, {14.8954f, 8.0f}, {16.0f, 8.0f}}},
    {PathVerb::Close, {{}, {}, {}}},
};

constexpr PathCommand kFlipHorizontalIconPath[] = {
    {PathVerb::MoveTo, {{4.0f, 5.0f}, {}, {}}},
    {PathVerb::LineTo, {{10.0f, 12.0f}, {}, {}}},
    {PathVerb::LineTo, {{4.0f, 19.0f}, {}, {}}},
    {PathVerb::LineTo, {{4.0f, 5.0f}, {}, {}}},

    {PathVerb::MoveTo, {{20.0f, 5.0f}, {}, {}}},
    {PathVerb::LineTo, {{14.0f, 12.0f}, {}, {}}},
    {PathVerb::LineTo, {{20.0f, 19.0f}, {}, {}}},
    {PathVerb::LineTo, {{20.0f, 5.0f}, {}, {}}},

    {PathVerb::MoveTo, {{12.0f, 3.5f}, {}, {}}},
    {PathVerb::LineTo, {{12.0f, 20.5f}, {}, {}}},
};

constexpr PathCommand kFlipVerticalIconPath[] = {
    {PathVerb::MoveTo, {{5.0f, 4.0f}, {}, {}}},
    {PathVerb::LineTo, {{12.0f, 10.0f}, {}, {}}},
    {PathVerb::LineTo, {{19.0f, 4.0f}, {}, {}}},
    {PathVerb::LineTo, {{5.0f, 4.0f}, {}, {}}},

    {PathVerb::MoveTo, {{5.0f, 20.0f}, {}, {}}},
    {PathVerb::LineTo, {{12.0f, 14.0f}, {}, {}}},
    {PathVerb::LineTo, {{19.0f, 20.0f}, {}, {}}},
    {PathVerb::LineTo, {{5.0f, 20.0f}, {}, {}}},

    {PathVerb::MoveTo, {{3.5f, 12.0f}, {}, {}}},
    {PathVerb::LineTo, {{20.5f, 12.0f}, {}, {}}},
};

constexpr PathCommand kColorPickerIconPath[] = {
    {PathVerb::MoveTo, {{14.5f, 4.5f}, {}, {}}},
    {PathVerb::LineTo, {{19.5f, 9.5f}, {}, {}}},
    {PathVerb::LineTo, {{16.6f, 12.4f}, {}, {}}},
    {PathVerb::LineTo, {{14.8f, 10.6f}, {}, {}}},
    {PathVerb::LineTo, {{7.2f, 18.2f}, {}, {}}},
    {PathVerb::LineTo, {{4.0f, 20.0f}, {}, {}}},
    {PathVerb::LineTo, {{5.8f, 16.8f}, {}, {}}},
    {PathVerb::LineTo, {{13.4f, 9.2f}, {}, {}}},
    {PathVerb::LineTo, {{11.6f, 7.4f}, {}, {}}},
    {PathVerb::LineTo, {{14.5f, 4.5f}, {}, {}}},

    {PathVerb::MoveTo, {{12.0f, 6.0f}, {}, {}}},
    {PathVerb::LineTo, {{18.0f, 12.0f}, {}, {}}},
};

} // namespace icons
