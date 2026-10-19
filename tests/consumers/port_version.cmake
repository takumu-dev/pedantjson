# Fails unless the vcpkg port's manifest MANIFEST gives the version VERSION, the project's, so that
# the port and project() can't drift apart.
file(READ "${MANIFEST}" manifest)
string(JSON port_version GET "${manifest}" version)
if(NOT port_version STREQUAL VERSION)
    message(FATAL_ERROR "${MANIFEST} gives version ${port_version}, and project() ${VERSION}")
endif()
