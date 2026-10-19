# Builds a dependency with Clang on Linux, taking the triplet's flags. It does what vcpkg's own
# Linux toolchain does for a native build, which a triplet can't keep while choosing the compiler.
include_guard(GLOBAL)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

string(APPEND CMAKE_C_FLAGS_INIT " -fPIC ${VCPKG_C_FLAGS}")
string(APPEND CMAKE_CXX_FLAGS_INIT " -fPIC ${VCPKG_CXX_FLAGS}")
string(APPEND CMAKE_C_FLAGS_DEBUG_INIT " ${VCPKG_C_FLAGS_DEBUG}")
string(APPEND CMAKE_CXX_FLAGS_DEBUG_INIT " ${VCPKG_CXX_FLAGS_DEBUG}")
string(APPEND CMAKE_C_FLAGS_RELEASE_INIT " ${VCPKG_C_FLAGS_RELEASE}")
string(APPEND CMAKE_CXX_FLAGS_RELEASE_INIT " ${VCPKG_CXX_FLAGS_RELEASE}")

foreach(kind IN ITEMS EXE SHARED MODULE)
    string(APPEND CMAKE_${kind}_LINKER_FLAGS_INIT " ${VCPKG_LINKER_FLAGS}")
    string(APPEND CMAKE_${kind}_LINKER_FLAGS_DEBUG_INIT " ${VCPKG_LINKER_FLAGS_DEBUG}")
    string(APPEND CMAKE_${kind}_LINKER_FLAGS_RELEASE_INIT " ${VCPKG_LINKER_FLAGS_RELEASE}")
endforeach()
