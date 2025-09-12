# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/appQTweaker_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/appQTweaker_autogen.dir/ParseCache.txt"
  "Core/CMakeFiles/Core_autogen.dir/AutogenUsed.txt"
  "Core/CMakeFiles/Core_autogen.dir/ParseCache.txt"
  "Core/Core_autogen"
  "NativeImage/CMakeFiles/NativeImage_autogen.dir/AutogenUsed.txt"
  "NativeImage/CMakeFiles/NativeImage_autogen.dir/ParseCache.txt"
  "NativeImage/NativeImage_autogen"
  "appQTweaker_autogen"
  )
endif()
