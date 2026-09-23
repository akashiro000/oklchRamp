cmake_minimum_required(VERSION 3.20)
project(oklchRamp LANGUAGES CXX)

# ---- Maya location ---------------------------------------------------------
# Override with -DMAYA_LOCATION=... or the MAYA_LOCATION environment variable.
if(DEFINED ENV{MAYA_LOCATION})
    set(_maya_default "$ENV{MAYA_LOCATION}")
elseif(WIN32)
    set(_maya_default "C:/Program Files/Autodesk/Maya2025")
elseif(APPLE)
    set(_maya_default "/Applications/Autodesk/maya2025/Maya.app/Contents")
else()
    set(_maya_default "/usr/autodesk/maya2025")
endif()
set(MAYA_LOCATION "${_maya_default}" CACHE PATH "Maya install root")
if(NOT EXISTS "${MAYA_LOCATION}/include/maya/MPxNode.h")
    message(FATAL_ERROR "Maya headers not found under MAYA_LOCATION=${MAYA_LOCATION}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(oklchRamp SHARED
    src/OklchColor.h
    src/OklchRampEval.h
    src/OklchRampNode.h
    src/OklchRampNode.cpp
    src/OklchRampSampleCmd.h
    src/OklchRampSampleCmd.cpp
    src/OklchRampWatchCmd.h
    src/OklchRampWatchCmd.cpp
    src/OklchRampFragments.h
    src/OklchRampOverride.h
    src/OklchRampOverride.cpp
    src/pluginMain.cpp
)

target_include_directories(oklchRamp PRIVATE "${MAYA_LOCATION}/include")
target_link_directories(oklchRamp PRIVATE "${MAYA_LOCATION}/lib")
target_link_libraries(oklchRamp PRIVATE Foundation OpenMaya OpenMayaRender)

if(WIN32)
    target_compile_definitions(oklchRamp PRIVATE
        NT_PLUGIN REQUIRE_IOSTREAM _BOOL NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_compile_options(oklchRamp PRIVATE /W4 /MD /EHsc)
    set_target_properties(oklchRamp PROPERTIES SUFFIX ".mll" PREFIX "")
    target_link_options(oklchRamp PRIVATE "/export:initializePlugin" "/export:uninitializePlugin")
elseif(APPLE)
    target_compile_definitions(oklchRamp PRIVATE OSMac_ REQUIRE_IOSTREAM _BOOL)
    set_target_properties(oklchRamp PROPERTIES SUFFIX ".bundle" PREFIX "")
else()
    target_compile_definitions(oklchRamp PRIVATE LINUX REQUIRE_IOSTREAM _BOOL)
    set_target_properties(oklchRamp PROPERTIES SUFFIX ".so" PREFIX "")
endif()

# Deploy the built plug-in into the module folder after every build.
# (Built in build/ first: Maya may still hold the previous .mll open, and a
#  rename-then-copy succeeds where a direct overwrite would fail.)
add_custom_command(TARGET oklchRamp POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC=$<TARGET_FILE:oklchRamp>
        -DDST=${CMAKE_SOURCE_DIR}/module/plug-ins/oklchRamp.mll
        -P ${CMAKE_SOURCE_DIR}/cmake/deploy.cmake
    COMMENT "Deploying oklchRamp.mll to module/plug-ins"
)

# ---- Arnold / MtoA translator (optional, built when MtoA is found) ----------
if(DEFINED ENV{MTOA_LOCATION})
    set(_mtoa_default "$ENV{MTOA_LOCATION}")
elseif(WIN32)
    set(_mtoa_default "C:/Program Files/Autodesk/Arnold/Maya2025")
else()
    set(_mtoa_default "/opt/autodesk/arnold/maya2025")
endif()
set(MTOA_LOCATION "${_mtoa_default}" CACHE PATH "MtoA install root")
if(EXISTS "${MTOA_LOCATION}/include/mtoa/extension/Extension.h")
    message(STATUS "MtoA found at ${MTOA_LOCATION}: building oklchRampTranslator")
    add_library(oklchRampTranslator SHARED
        src/OklchColor.h
        src/OklchRampEval.h
        src/arnold/OklchRampTranslator.cpp
    )
    target_include_directories(oklchRampTranslator PRIVATE
        "${MAYA_LOCATION}/include"
        "${MTOA_LOCATION}/include/mtoa"
        "${MTOA_LOCATION}/include/arnold"
    )
    target_link_directories(oklchRampTranslator PRIVATE "${MAYA_LOCATION}/lib" "${MTOA_LOCATION}/lib")
    target_link_libraries(oklchRampTranslator PRIVATE mtoa_api ai Foundation OpenMaya)
    if(WIN32)
        target_compile_definitions(oklchRampTranslator PRIVATE
            NT_PLUGIN REQUIRE_IOSTREAM _BOOL NOMINMAX _CRT_SECURE_NO_WARNINGS)
        target_compile_options(oklchRampTranslator PRIVATE /W3 /MD /EHsc)
    endif()
    set_target_properties(oklchRampTranslator PROPERTIES PREFIX "")
    add_custom_command(TARGET oklchRampTranslator POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DSRC=$<TARGET_FILE:oklchRampTranslator>
            -DDST=${CMAKE_SOURCE_DIR}/module/arnold/oklchRampTranslator.dll
            -P ${CMAKE_SOURCE_DIR}/cmake/deploy.cmake
        COMMENT "Deploying oklchRampTranslator.dll to module/arnold"
    )
else()
    message(STATUS "MtoA not found (MTOA_LOCATION=${MTOA_LOCATION}); skipping Arnold translator")
endif()
