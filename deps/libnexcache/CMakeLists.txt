cmake_minimum_required(VERSION 3.7.0)

macro(getDefinedVersion name)
  set(VERSION_REGEX "^#define LIBNEXCACHE_${name} (.+)$")
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/nexcache/nexcache.h"
    MATCHED_LINE REGEX ${VERSION_REGEX})
  string(REGEX REPLACE ${VERSION_REGEX} "\\1" ${name} "${MATCHED_LINE}")
endmacro()

getDefinedVersion(VERSION_MAJOR)
getDefinedVersion(VERSION_MINOR)
getDefinedVersion(VERSION_PATCH)
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
message("Detected libnexcache version: ${VERSION}")

project(libnexcache LANGUAGES "C" VERSION "${VERSION}")
INCLUDE(GNUInstallDirs)

option(ENABLE_THREADS "Enable thread-safe initialization" ON)
OPTION(BUILD_SHARED_LIBS "Build shared libraries" ON)
OPTION(ENABLE_TLS "Build nexcache_tls for TLS support" OFF)
OPTION(DISABLE_TESTS "If tests should be compiled or not" OFF)
OPTION(ENABLE_EXAMPLES "Enable building nexcache examples" OFF)
option(ENABLE_IPV6_TESTS "Enable IPv6 tests requiring special prerequisites" OFF)
OPTION(ENABLE_RDMA "Build nexcache_rdma for RDMA support" OFF)
OPTION(ENABLE_DLOPEN_RDMA "Build nexcache_rdma with dynamic loading" OFF)

# Libnexcache requires C99 (-std=c99)
SET(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
SET(CMAKE_DEBUG_POSTFIX d)

# Set target-common flags
if(NOT WIN32)
  add_compile_options(-Werror -Wall -Wextra -pedantic)
  add_compile_options(-Wstrict-prototypes -Wwrite-strings -Wno-missing-field-initializers)
else()
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN)
endif()

set(nexcache_sources
    src/adlist.c
    src/alloc.c
    src/async.c
    src/cluster.c
    src/command.c
    src/conn.c
    src/crc16.c
    src/net.c
    src/read.c
    src/sockcompat.c
    src/nexcache.c
    src/vkutil.c)

# Allow the libnexcache provided sds and dict types to be replaced by
# compatible implementations (like NexCache's).
# A replaced type is not included in a built archive or shared library.
if(NOT DICT_INCLUDE_DIR)
  set(nexcache_sources ${nexcache_sources} src/dict.c)
  set(DICT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()
if(NOT SDS_INCLUDE_DIR)
  set(nexcache_sources ${nexcache_sources} src/sds.c)
  set(SDS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

ADD_LIBRARY(nexcache ${nexcache_sources})
ADD_LIBRARY(nexcache::nexcache ALIAS nexcache)
set_target_properties(nexcache PROPERTIES
  C_VISIBILITY_PRESET hidden
  WINDOWS_EXPORT_ALL_SYMBOLS TRUE
  SOVERSION "${VERSION_MAJOR}"
  VERSION "${VERSION}")

if(MSVC)
  # Produce object files that contain debug info.
  target_compile_options(nexcache PRIVATE /Z7)
endif()

set(nexcache_link_libraries)
set(nexcache_compile_definitions)

IF(WIN32)
    list(APPEND nexcache_link_libraries ws2_32 crypt32)
ELSEIF(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    list(APPEND nexcache_link_libraries m)
ELSEIF(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    list(APPEND nexcache_link_libraries socket)
ENDIF()

if(ENABLE_THREADS)
    find_package(Threads REQUIRED)
    list(APPEND nexcache_link_libraries Threads::Threads)
    list(APPEND nexcache_compile_definitions NEXCACHE_USE_THREADS)
endif()

if(nexcache_link_libraries)
    target_link_libraries(nexcache PUBLIC ${nexcache_link_libraries})
endif()

if(nexcache_compile_definitions)
    target_compile_definitions(nexcache PRIVATE ${nexcache_compile_definitions})
endif()

TARGET_INCLUDE_DIRECTORIES(nexcache
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/nexcache>
    PRIVATE
        $<BUILD_INTERFACE:${DICT_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${SDS_INCLUDE_DIR}>
)

CONFIGURE_FILE(nexcache.pc.in nexcache.pc @ONLY)

set(CPACK_PACKAGE_VENDOR "NexCache")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Minimalistic C client library for NexCache")
set(CPACK_PACKAGE_DESCRIPTION "\
Libnexcache is a minimalistic C client library for the NexCache, KeyDB, and NexCache databases.

It is minimalistic because it just adds minimal support for the protocol, \
but at the same time it uses a high level printf-alike API in order to make \
it much higher level than otherwise suggested by its minimal code base and the \
lack of explicit bindings for every server command.

Apart from supporting sending commands and receiving replies, it comes with a \
reply parser that is decoupled from the I/O layer. It is a stream parser designed \
for easy reusability, which can for instance be used in higher level language bindings \
for efficient reply parsing.

Libnexcache only supports the binary-safe RESP protocol, so you can use it with any NexCache \
compatible server >= 1.2.0.

The library comes with multiple APIs. There is the synchronous API, the asynchronous API \
and the reply parsing API.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/nexcache-io/libnexcache")
set(CPACK_PACKAGE_CONTACT "michael dot grunder at gmail dot com")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_GROUP "Productivity/Databases/Clients")
set(CPACK_RPM_PACKAGE_LICENSE "BSD-3-Clause")
include(CPack)

INSTALL(TARGETS nexcache
    EXPORT nexcache-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(MSVC AND BUILD_SHARED_LIBS)
  # Install linker generated program database file.
  install(FILES $<TARGET_PDB_FILE:nexcache>
          DESTINATION ${CMAKE_INSTALL_BINDIR}
          CONFIGURATIONS Debug RelWithDebInfo)
endif()

# Install public headers
install(DIRECTORY include/nexcache
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PATTERN "tls.h" EXCLUDE
        PATTERN "rdma.h" EXCLUDE
        PATTERN "adapters/macosx.h" EXCLUDE)
if(APPLE)
  install(FILES include/nexcache/adapters/macosx.h
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nexcache/adapters)
endif()

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/nexcache.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

export(EXPORT nexcache-targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/nexcache-targets.cmake"
    NAMESPACE nexcache::)

if(WIN32)
    SET(CMAKE_CONF_INSTALL_DIR share/nexcache)
else()
    SET(CMAKE_CONF_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/nexcache)
endif()
SET(INCLUDE_INSTALL_DIR include)
include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/nexcache-config-version.cmake"
                                 COMPATIBILITY SameMajorVersion)
configure_package_config_file(nexcache-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/nexcache-config.cmake
                              INSTALL_DESTINATION ${CMAKE_CONF_INSTALL_DIR}
                              PATH_VARS INCLUDE_INSTALL_DIR)

INSTALL(EXPORT nexcache-targets
        FILE nexcache-targets.cmake
        NAMESPACE nexcache::
        DESTINATION ${CMAKE_CONF_INSTALL_DIR})

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/nexcache-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/nexcache-config-version.cmake
        DESTINATION ${CMAKE_CONF_INSTALL_DIR})


IF(ENABLE_TLS)
    IF (NOT OPENSSL_ROOT_DIR)
        IF (APPLE)
            SET(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
        ENDIF()
    ENDIF()
    FIND_PACKAGE(OpenSSL REQUIRED)
    SET(nexcache_tls_sources
        src/tls.c)
    ADD_LIBRARY(nexcache_tls ${nexcache_tls_sources})
    ADD_LIBRARY(nexcache::nexcache_tls ALIAS nexcache_tls)

    TARGET_INCLUDE_DIRECTORIES(nexcache_tls
        PRIVATE
            $<INSTALL_INTERFACE:include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/nexcache>
            $<BUILD_INTERFACE:${DICT_INCLUDE_DIR}>
            $<BUILD_INTERFACE:${SDS_INCLUDE_DIR}>
    )

    set_target_properties(nexcache_tls PROPERTIES
      C_VISIBILITY_PRESET hidden
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
      SOVERSION "${VERSION_MAJOR}"
      VERSION "${VERSION}")
    if(MSVC)
      # Produce object files that contain debug info.
      target_compile_options(nexcache_tls PRIVATE /Z7)
    endif()
    target_link_libraries(nexcache_tls PRIVATE nexcache::nexcache OpenSSL::SSL)
    CONFIGURE_FILE(nexcache_tls.pc.in nexcache_tls.pc @ONLY)

    INSTALL(TARGETS nexcache_tls
        EXPORT nexcache_tls-targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    # Install public header
    install(FILES include/nexcache/tls.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nexcache)

    if(MSVC AND BUILD_SHARED_LIBS)
      # Install linker generated program database file.
      install(FILES $<TARGET_PDB_FILE:nexcache_tls>
              DESTINATION ${CMAKE_INSTALL_BINDIR}
              CONFIGURATIONS Debug RelWithDebInfo)
    endif()

    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/nexcache_tls.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

    export(EXPORT nexcache_tls-targets
           FILE "${CMAKE_CURRENT_BINARY_DIR}/nexcache_tls-targets.cmake"
           NAMESPACE nexcache::)

    if(WIN32)
        SET(CMAKE_CONF_INSTALL_DIR share/nexcache_tls)
    else()
        SET(CMAKE_CONF_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/nexcache_tls)
    endif()
    configure_package_config_file(nexcache_tls-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/nexcache_tls-config.cmake
                                  INSTALL_DESTINATION ${CMAKE_CONF_INSTALL_DIR}
                                  PATH_VARS INCLUDE_INSTALL_DIR)

    INSTALL(EXPORT nexcache_tls-targets
        FILE nexcache_tls-targets.cmake
        NAMESPACE nexcache::
        DESTINATION ${CMAKE_CONF_INSTALL_DIR})

    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/nexcache_tls-config.cmake
        DESTINATION ${CMAKE_CONF_INSTALL_DIR})
ENDIF()

if(ENABLE_RDMA)
    set(nexcache_rdma_sources src/rdma.c)
    add_library(nexcache_rdma ${nexcache_rdma_sources})
    add_library(nexcache::nexcache_rdma ALIAS nexcache_rdma)
    
    if (ENABLE_DLOPEN_RDMA)
        message(STATUS "libnexcache: Building RDMA with dynamic loading (dlopen)")
        target_compile_definitions(nexcache_rdma PRIVATE DLOPEN_RDMA)
    else()
        message(STATUS "libnexcache: Building RDMA with static/hard linking")
        find_library(RDMACM_LIBRARIES rdmacm REQUIRED)
        find_library(IBVERBS_LIBRARIES ibverbs REQUIRED)
        target_link_libraries(nexcache_rdma LINK_PRIVATE ${RDMACM_LIBRARIES} ${IBVERBS_LIBRARIES})
    endif()

    target_include_directories(nexcache_rdma
        PRIVATE
            $<INSTALL_INTERFACE:include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/nexcache>
            $<BUILD_INTERFACE:${DICT_INCLUDE_DIR}>
            $<BUILD_INTERFACE:${SDS_INCLUDE_DIR}>
    )

    set_target_properties(nexcache_rdma PROPERTIES
      C_VISIBILITY_PRESET hidden
      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
      SOVERSION "${VERSION_MAJOR}"
      VERSION "${VERSION}")
    configure_file(nexcache_rdma.pc.in nexcache_rdma.pc @ONLY)

    install(TARGETS nexcache_rdma
        EXPORT nexcache_rdma-targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    # Install public header
    install(FILES include/nexcache/rdma.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nexcache)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/nexcache_rdma.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

    export(EXPORT nexcache_rdma-targets
           FILE "${CMAKE_CURRENT_BINARY_DIR}/nexcache_rdma-targets.cmake"
           NAMESPACE nexcache::)

    set(CMAKE_CONF_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/nexcache_rdma)
    configure_package_config_file(nexcache_rdma-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/nexcache_rdma-config.cmake
                                  INSTALL_DESTINATION ${CMAKE_CONF_INSTALL_DIR}
                                  PATH_VARS INCLUDE_INSTALL_DIR)

    install(EXPORT nexcache_rdma-targets
        FILE nexcache_rdma-targets.cmake
        NAMESPACE nexcache::
        DESTINATION ${CMAKE_CONF_INSTALL_DIR})

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/nexcache_rdma-config.cmake
        DESTINATION ${CMAKE_CONF_INSTALL_DIR})
endif()

# Add tests
if(NOT DISABLE_TESTS)
  # Unit tests uses a static library to ensure all symbols are visible.
  # This single library also bundles TLS and RDMA when enabled.
  add_library(nexcache_unittest STATIC ${nexcache_sources} ${nexcache_tls_sources} ${nexcache_rdma_sources})

  # Mirror the include directories.
  get_target_property(include_directories nexcache::nexcache INCLUDE_DIRECTORIES)
  target_include_directories(nexcache_unittest PUBLIC ${include_directories})

  if(nexcache_compile_definitions)
    target_compile_definitions(nexcache_unittest PRIVATE ${nexcache_compile_definitions})
  endif()
  if(nexcache_link_libraries)
    target_link_libraries(nexcache_unittest PUBLIC ${nexcache_link_libraries})
  endif()
  if(ENABLE_TLS)
    target_link_libraries(nexcache_unittest PRIVATE OpenSSL::SSL)
  endif()
  if(ENABLE_RDMA)
    target_link_libraries(nexcache_unittest PRIVATE ${RDMACM_LIBRARIES} ${IBVERBS_LIBRARIES})
  endif()

  # Create libnexcache_unittest.a in the tests directory.
  set_target_properties(nexcache_unittest PROPERTIES
                        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")

  # Make sure ctest prints the output when a test fails.
  set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
  include(CTest)
  add_subdirectory(tests)
endif()

# Add examples
IF(ENABLE_EXAMPLES)
    ADD_SUBDIRECTORY(examples)
ENDIF(ENABLE_EXAMPLES)
