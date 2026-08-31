cmake_minimum_required(VERSION 3.24)
project(FailureFabric VERSION 1.0.0 LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_EXTENSIONS OFF)
set(CMAKE_WARNINGS_AS_ERRORS ON)

option(FF_ENABLE_CUDA "Build CUDA failure/recovery proofs" ON)
option(FF_BUILD_TESTS "Build tests" ON)
option(FF_BUILD_EXAMPLES "Build examples" ON)
option(FF_BUILD_BENCHMARKS "Build benchmarks" ON)
option(FF_BUILD_CLI "Build the failure-fabric CLI" ON)

find_package(CUDAToolkit REQUIRED)

file(GLOB FF_CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
if(FF_ENABLE_CUDA)
  file(GLOB FF_CUDA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cu)
  set_property(DIRECTORY PROPERTY CMAKE_CUDA_ARCHITECTURES 120)
endif()

add_library(FailureFabric ${FF_CORE_SOURCES} ${FF_CUDA_SOURCES})
add_library(ff::FailureFabric ALIAS FailureFabric)
target_include_directories(FailureFabric PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_include_directories(FailureFabric PRIVATE ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
target_compile_definitions(FailureFabric PRIVATE _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_definitions(FailureFabric PUBLIC FF_USE_WINSOCK=1)
target_compile_options(FailureFabric PRIVATE /W4 /WX /permissive-)
target_link_libraries(FailureFabric PUBLIC CUDA::cudart CUDA::cuda_driver)
target_link_libraries(FailureFabric PUBLIC ws2_32)
if(FF_ENABLE_CUDA)
  target_compile_options(FailureFabric PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/W4 -Xcompiler=/WX -Xcompiler=/permissive- --expt-relaxed-constexpr>)
endif()

set_target_properties(FailureFabric PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
  CUDA_ARCHITECTURES 120
  CUDA_STANDARD 20)

# ---- tests ----
if(FF_BUILD_TESTS)
  enable_testing()
  file(GLOB FF_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  foreach(t ${FF_TEST_SOURCES})
    get_filename_component(tname ${t} NAME_WE)
    add_executable(${tname} ${t})
    target_include_directories(${tname} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(${tname} PRIVATE ff::FailureFabric)
    target_compile_options(${tname} PRIVATE /W4 /WX /permissive-)
    target_compile_definitions(${tname} PRIVATE _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME ${tname} COMMAND ${tname})
  endforeach()
endif()

# ---- examples ----
if(FF_BUILD_EXAMPLES)
  file(GLOB FF_EXAMPLE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp)
  foreach(t ${FF_EXAMPLE_SOURCES})
    get_filename_component(tname ${t} NAME_WE)
    add_executable(${tname} ${t})
    target_include_directories(${tname} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(${tname} PRIVATE ff::FailureFabric)
    target_compile_options(${tname} PRIVATE /W4 /WX /permissive-)
    target_compile_definitions(${tname} PRIVATE _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
  endforeach()
endif()

# ---- benchmarks ----
if(FF_BUILD_BENCHMARKS)
  file(GLOB FF_BENCH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp)
  foreach(t ${FF_BENCH_SOURCES})
    get_filename_component(tname ${t} NAME_WE)
    add_executable(${tname} ${t})
    target_include_directories(${tname} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(${tname} PRIVATE ff::FailureFabric)
    target_compile_options(${tname} PRIVATE /W4 /WX /permissive-)
    target_compile_definitions(${tname} PRIVATE _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
  endforeach()
endif()

# ---- CLI ----
if(FF_BUILD_CLI)
  add_executable(ff_cli tools/ff_cli.cpp)
  target_include_directories(ff_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(ff_cli PRIVATE ff::FailureFabric)
  target_compile_options(ff_cli PRIVATE /W4 /WX /permissive-)
  target_compile_definitions(ff_cli PRIVATE _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# ---- install / package ----
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS FailureFabric
  EXPORT FailureFabricTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(FF_INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/FailureFabric)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/FailureFabricConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FailureFabricConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/FailureFabricConfig.cmake"
  INSTALL_DESTINATION ${FF_INSTALL_CONFIGDIR})

install(EXPORT FailureFabricTargets
  FILE FailureFabricTargets.cmake
  NAMESPACE ff::
  DESTINATION ${FF_INSTALL_CONFIGDIR})
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/FailureFabricConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/FailureFabricConfigVersion.cmake"
  DESTINATION ${FF_INSTALL_CONFIGDIR})
