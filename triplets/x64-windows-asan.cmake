# x64-windows-static-md with AddressSanitizer, for the msvc-asan preset. MSVC's standard library
# won't link instrumented code beside code that isn't, so the tests' dependencies are instrumented
# too. AddressSanitizer refuses the /RTC1 that vcpkg puts in every debug build, so only release
# libraries are built, and the preset builds against the release CRT. As in x64-windows-static-md,
# the toolset is the developer command prompt's when vcpkg passes VCToolsVersion through.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)

set(VCPKG_C_FLAGS "/fsanitize=address")
set(VCPKG_CXX_FLAGS "/fsanitize=address")

if(DEFINED ENV{VCToolsVersion})
    set(VCPKG_PLATFORM_TOOLSET_VERSION "$ENV{VCToolsVersion}")
endif()
