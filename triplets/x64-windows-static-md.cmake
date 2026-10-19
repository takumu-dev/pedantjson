# vcpkg's x64-windows-static-md, built by the MSVC toolset of the developer command prompt vcpkg
# runs in, when vcpkg passes its VCToolsVersion through (VCPKG_KEEP_ENV_VARS). A static library built
# by a newer toolset than the one that links it may not link, so CI builds the tests' dependencies
# with the toolset each job tests, the oldest included.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

if(DEFINED ENV{VCToolsVersion})
    set(VCPKG_PLATFORM_TOOLSET_VERSION "$ENV{VCToolsVersion}")
endif()
