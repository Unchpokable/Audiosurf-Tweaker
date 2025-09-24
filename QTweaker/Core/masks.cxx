#include "precompiled.hxx"

#include "masks.hxx"

std::vector<const char*> core::masks::required_files {
    "cliff1-1.png",
    "cliff1-2.png",
    "cliff2-1.png",
    "cliff2-2.png",
    "hit1.png",
    "hit2.jpg",
    "particles1.png",
    "particles2.jpg",
    "particles3.jpg",
    "ring1A.png",
    "ring1B.png",
    "ring2A.jpg",
    "ring2B.jpg",
    "tilesflyup.png",
    "tiles.png",
};

std::vector<const char*> core::masks::optional_files {
    "Skysphere_Black.png",
    "Skysphere_Grey.png",
    "Skysphere_White.png",
};

QRegularExpression core::masks::preview_screenshots = QRegularExpression(R"(^PreviewScreenshot.*\.(png|jpg|jpeg)$)",
    QRegularExpression::CaseInsensitiveOption);
