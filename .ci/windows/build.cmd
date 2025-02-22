cmake.exe ^
  -S . ^
  -B build ^
  -G "Visual Studio 17 2022" ^
  -C ".ci\debug-flags.cmake" ^
  -DCAF_ENABLE_ROBOT_TESTS=ON ^
  -DCAF_LOG_LEVEL=TRACE ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DCMAKE_C_COMPILER=cl.exe ^
  -DCMAKE_CXX_COMPILER=cl.exe ^
  -DCMAKE_CXX_FLAGS="/WX"

cmake.exe --build build --parallel %NUMBER_OF_PROCESSORS% --target install --config debug || exit \b 1
