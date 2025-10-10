#ifndef MASKS_H
#define MASKS_H

#include "Core_global.h"

namespace core::masks
{
CORE_EXPORT extern std::vector<const char*> required_files;
CORE_EXPORT extern std::vector<const char*> optional_files;
CORE_EXPORT extern QRegularExpression preview_screenshots;
} // namespace core::masks

namespace core::masks
{
CORE_EXPORT extern std::vector<const char*> cliffs;
CORE_EXPORT extern std::vector<const char*> hits;
CORE_EXPORT extern std::vector<const char*> particles;
CORE_EXPORT extern std::vector<const char*> rings;
CORE_EXPORT extern std::vector<const char*> skyspheres;
} // namespace core::masks

#endif
