if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_INSTALL_INCLUDEDIR include/adhoc-server CACHE PATH "")
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
set(package adhoc-server)

install(
    TARGETS adhoc-server_adhoc-server
    EXPORT adhoc-serverTargets
    RUNTIME COMPONENT adhoc-server_Runtime
)

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

# Allow package maintainers to freely override the path for the configs
set(
    adhoc-server_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATADIR}/${package}"
    CACHE PATH "CMake package config location relative to the install prefix"
)
mark_as_advanced(adhoc-server_INSTALL_CMAKEDIR)

install(
    FILES cmake/install-config.cmake
    DESTINATION "${adhoc-server_INSTALL_CMAKEDIR}"
    RENAME "${package}Config.cmake"
    COMPONENT adhoc-server_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${adhoc-server_INSTALL_CMAKEDIR}"
    COMPONENT adhoc-server_Development
)

install(
    EXPORT adhoc-serverTargets
    NAMESPACE adhoc-server::
    DESTINATION "${adhoc-server_INSTALL_CMAKEDIR}"
    COMPONENT adhoc-server_Development
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
