project(nexcache-server)

set(INSTALL_BIN_PATH ${CMAKE_INSTALL_PREFIX}/bin)
set_directory_properties(PROPERTIES CLEAN_NO_CUSTOM 1)

# Target: nexcache-server
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NEXCACHE_SERVER_CFLAGS}")
message(STATUS "CFLAGS: ${CMAKE_C_FLAGS}")

get_nexcache_server_linker_option(NEXCACHE_SERVER_LDFLAGS)
list(APPEND SERVER_LIBS "fpconv")
list(APPEND SERVER_LIBS "hdr_histogram")
nexcache_build_and_install_bin(nexcache-server "${NEXCACHE_SERVER_SRCS}" "${NEXCACHE_SERVER_LDFLAGS}" "${SERVER_LIBS}"
                             "nexcache-server")
add_dependencies(nexcache-server generate_commands_def)
add_dependencies(nexcache-server generate_fmtargs_h)
add_dependencies(nexcache-server release_header)

if (BUILD_LUA)
    message(STATUS "Build Lua scripting engine module")
    add_subdirectory(modules/lua)
    add_dependencies(nexcache-server nexcachelua)
    target_compile_definitions(nexcache-server PRIVATE LUA_ENABLED)
    if (UNIX AND NOT APPLE)
        target_compile_definitions(nexcache-server PRIVATE LUA_LIB=libnexcachelua.so)
        target_link_options(nexcache-server PRIVATE -Wl,--disable-new-dtags)
    else ()
        target_compile_definitions(nexcache-server PRIVATE LUA_LIB=libnexcachelua.dylib)
    endif ()
    set(NEXCACHE_INSTALL_RPATH "")
    set_target_properties(nexcache-server PROPERTIES
        INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR};${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
        INSTALL_RPATH_USE_LINK_PATH TRUE
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()
unset(BUILD_LUA CACHE)

if (NEXCACHE_RELEASE_BUILD)
    # Enable LTO for Release build
    set_property(TARGET nexcache-server PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

if (DEBUG_FORCE_DEFRAG)
    message(STATUS "Forcing Active Defrag run on nexcache-server")
    target_compile_definitions(nexcache-server PRIVATE DEBUG_FORCE_DEFRAG)
    target_compile_definitions(nexcache-server PRIVATE HAVE_DEFRAG)
endif ()

if (BUILD_SANITIZER)
    # 'BUILD_SANITIZER' is defined in NexCacheSetup module (based on user input)
    # If defined, the variables 'NEXCACHE_SANITAIZER_CFLAGS' and 'NEXCACHE_SANITAIZER_LDFLAGS'
    # are set with the link & compile flags required
    message(STATUS "Adding sanitizer flags for target nexcache-server")
    target_compile_options(nexcache-server PRIVATE ${NEXCACHE_SANITAIZER_CFLAGS})
    target_link_options(nexcache-server PRIVATE ${NEXCACHE_SANITAIZER_LDFLAGS})
endif ()
unset(BUILD_SANITIZER CACHE)

# Target: nexcache-cli
list(APPEND CLI_LIBS "fpconv")
list(APPEND CLI_LIBS "linenoise")
nexcache_build_and_install_bin(nexcache-cli "${NEXCACHE_CLI_SRCS}" "${NEXCACHE_SERVER_LDFLAGS}" "${CLI_LIBS}" "nexcache-cli")
add_dependencies(nexcache-cli generate_commands_def)
add_dependencies(nexcache-cli generate_fmtargs_h)

# Target: nexcache-benchmark
list(APPEND BENCH_LIBS "fpconv")
list(APPEND BENCH_LIBS "hdr_histogram")
nexcache_build_and_install_bin(nexcache-benchmark "${NEXCACHE_BENCHMARK_SRCS}" "${NEXCACHE_SERVER_LDFLAGS}" "${BENCH_LIBS}"
                             "nexcache-benchmark")
add_dependencies(nexcache-benchmark generate_commands_def)
add_dependencies(nexcache-benchmark generate_fmtargs_h)

# Targets: nexcache-sentinel, nexcache-check-aof and nexcache-check-rdb are just symbolic links
nexcache_create_symlink("nexcache-server" "nexcache-sentinel")
nexcache_create_symlink("nexcache-server" "nexcache-check-rdb")
nexcache_create_symlink("nexcache-server" "nexcache-check-aof")

# Target nexcache-rdma
if (BUILD_RDMA_MODULE)
    set(MODULE_NAME "nexcache-rdma")
    message(STATUS "Building RDMA module")
    add_library(${MODULE_NAME} SHARED "${NEXCACHE_RDMA_MODULE_SRCS}")
    target_compile_options(${MODULE_NAME} PRIVATE -DBUILD_RDMA_MODULE=2 -DUSE_RDMA=1)
    target_link_libraries(${MODULE_NAME} "${RDMA_LIBS}")
    # remove the "lib" prefix from the module
    set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
    nexcache_install_bin(${MODULE_NAME})
endif ()

# Target nexcache-tls (a module)
if (BUILD_TLS_MODULE)
    message(STATUS "Building TLS as a module")
    set(MODULE_NAME "nexcache-tls")
    add_library(${MODULE_NAME} SHARED ${NEXCACHE_TLS_MODULE_SRCS})
    target_compile_options(${MODULE_NAME} PRIVATE -DUSE_OPENSSL=2 -DBUILD_TLS_MODULE=2)
    if (APPLE)
        # Some symbols can only be resolved during runtime (they exist in the executable)
        target_link_options(${MODULE_NAME} PRIVATE -undefined dynamic_lookup)
    endif ()
    target_link_libraries(${MODULE_NAME} nexcache::nexcache_tls OpenSSL::SSL)
    set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
endif ()

if (BUILD_EXAMPLE_MODULES)
    # Include the modules ("hello*")
    message(STATUS "Building example modules")
    add_subdirectory(modules)
endif ()

if (BUILD_UNIT_GTESTS)
    add_subdirectory(unit)
endif ()

# Friendly hint like the Makefile one
file(RELATIVE_PATH _CMAKE_DIR_RELATIVE_PATH "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}")
add_custom_target(hint ALL
    DEPENDS nexcache-server nexcache-cli nexcache-benchmark
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Hint: It is a good idea to run tests with your CMake-built binaries \\;\\)"
    COMMAND ${CMAKE_COMMAND} -E echo "      ./${_CMAKE_DIR_RELATIVE_PATH}/runtest"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)
