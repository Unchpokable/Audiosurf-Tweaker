#include "precompiled.hxx"

#include "masks.hxx"

namespace
{
constexpr const char* cliff11 = "cliff1-1.png";
constexpr const char* cliff12 = "cliff1-2.png";
constexpr const char* cliff21 = "cliff2-1.png";
constexpr const char* cliff22 = "cliff2-2.png";
//
constexpr const char* hit1 = "hit1.png";
constexpr const char* hit2 = "hit2.jpg";
//
constexpr const char* particles1 = "particles1.png";
constexpr const char* particles2 = "particles2.jpg";
constexpr const char* particles3 = "particles3.jpg";
//
constexpr const char* ring1A = "ring1A.png";
constexpr const char* ring1B = "ring1B.png";
constexpr const char* ring2A = "ring2A.jpg";
constexpr const char* ring2B = "ring2B.jpg";
//
constexpr const char* tiles = "tiles.png";
constexpr const char* tiles_flyup = "tilesflyup.png";
//
constexpr const char* skysphere_black = "Skysphere_Black.png";
constexpr const char* skysphere_grey = "Skysphere_Grey.png";
constexpr const char* skysphere_white = "Skysphere_White.png";
} // namespace

std::vector<const char*> core::masks::required_files {
    cliff11,
    cliff12,
    cliff21,
    cliff22,
    hit1,
    hit2,
    particles1,
    particles2,
    particles3,
    ring1A,
    ring1B,
    ring2A,
    ring2B,
    tiles_flyup,
    tiles,
};

std::vector<const char*> core::masks::optional_files {
    skysphere_black,
    skysphere_grey,
    skysphere_white,
};

QRegularExpression core::masks::preview_screenshots =
    QRegularExpression(R"(^PreviewScreenshot.*\.(png|jpg|jpeg)$)", QRegularExpression::CaseInsensitiveOption);

std::vector<const char*> core::masks::cliffs {
    cliff11,
    cliff12,
    cliff21,
    cliff22,
};

std::vector<const char*> core::masks::hits {
    hit1,
    hit2,
};

std::vector<const char*> core::masks::particles {
    particles1,
    particles2,
    particles3,
};

std::vector<const char*> core::masks::rings {
    ring1A,
    ring1B,
    ring2A,
    ring2B,
};

std::vector<const char*> core::masks::skyspheres {
    skysphere_black,
    skysphere_grey,
    skysphere_white,
};
