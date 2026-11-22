# Reads the project version from the top-level VERSION file so that CMake,
# the plugin binaries and the CI artefacts all share a single source of truth.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../VERSION" OE_VERSION LIMIT_COUNT 1)
string(STRIP "${OE_VERSION}" OE_VERSION)

if(NOT OE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION file must contain MAJOR.MINOR.PATCH, got '${OE_VERSION}'")
endif()
