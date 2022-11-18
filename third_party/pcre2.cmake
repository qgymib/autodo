# PCRE2's CMakeLists.txt does not use target-based syntax, so we cannot just
# `add_subdirectory()` for it.

include (ExternalProject)
include(GNUInstallDirs)

set(PCRE2_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pcre2)
set(PCRE2_INSTALL_PATH ${CMAKE_CURRENT_BINARY_DIR}/third_party/pcre2)

if (WIN32)
    set(PCRE2_LIB_PATH  ${PCRE2_INSTALL_PATH}/lib/pcre2-8-static.lib)
else ()
    set(PCRE2_LIB_PATH  ${PCRE2_INSTALL_PATH}/${CMAKE_INSTALL_LIBDIR}/libpcre2-8.a)
endif ()

ExternalProject_Add(3rd_pcre2
    SOURCE_DIR          ${PCRE2_SOURCE_PATH}
    CMAKE_ARGS          -DCMAKE_BUILD_TYPE=Release
                        -DCMAKE_INSTALL_PREFIX:PATH=${PCRE2_INSTALL_PATH}
                        -DPCRE2_SUPPORT_JIT=ON
                        -DPCRE2_BUILD_TESTS=OFF
                        -DPCRE2_BUILD_PCRE2GREP=OFF
                        -DBUILD_SHARED_LIBS=OFF
                        -DPCRE2_SHOW_REPORT=OFF
    BUILD_BYPRODUCTS    ${PCRE2_LIB_PATH})

# INTERFACE_INCLUDE_DIRECTORIES requires include path exists.
file(MAKE_DIRECTORY ${PCRE2_INSTALL_PATH}/include)

# Export target
add_library(pcre2 STATIC IMPORTED GLOBAL)
set_target_properties(pcre2 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${PCRE2_INSTALL_PATH}/include
    IMPORTED_LOCATION ${PCRE2_LIB_PATH})

# Target `pcre2` dependent on project `3rd_pcre2`
add_dependencies(pcre2 3rd_pcre2)
