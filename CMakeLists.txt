cmake_minimum_required(VERSION 2.8)

project(SceneExporter C CXX)

# Add a sensible default build type
if (NOT CMAKE_BUILD_TYPE)
  message(WARNING "CMAKE_BUILD_TYPE is not defined; defaulting to Debug.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
      "Build type: None Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif()

if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic")
  add_definitions(-std=c++11)
endif()

set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "No assimp tools")
set(ASSIMP_BUILD_STATIC_LIB ON CACHE BOOL "Static assimp")
set(ENABLE_BOOST_WORKAROUND ON CACHE BOOL "No boost")

add_subdirectory(deps)

include_directories(
	${Assimp_SOURCE_DIR}/include
)

file(GLOB_RECURSE ALL_SRCS
	RELATIVE ${CMAKE_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp)

add_executable(SceneExporter ${ALL_SRCS}) 

LINK_DIRECTORIES(${Assimp_BINARY_DIR} ${AssetImporter_BINARY_DIR}/lib)
TARGET_LINK_LIBRARIES(SceneExporter assimp)