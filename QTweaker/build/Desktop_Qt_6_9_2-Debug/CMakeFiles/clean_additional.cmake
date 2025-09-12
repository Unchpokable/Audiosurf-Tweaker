# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/appQTweaker_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/appQTweaker_autogen.dir/ParseCache.txt"
  "appQTweaker_autogen"
  )
endif()
