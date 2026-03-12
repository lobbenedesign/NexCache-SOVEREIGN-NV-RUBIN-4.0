# Set the directory to find NexCache binaries for tests. Historically we've been
# using make to build binaries under the src/ directory. Since we start supporting
# CMake as well, we allow changing base dir by passing ENV variable `NEXCACHE_BIN_DIR`,
# which could be either absolute or relative path (e.g. cmake-build-debug/bin).
if {[info exists ::env(NEXCACHE_BIN_DIR)]} {
    set ::NEXCACHE_BIN_DIR [file normalize $::env(NEXCACHE_BIN_DIR)]
} else {
    set ::NEXCACHE_BIN_DIR "[pwd]/src"
}

# Optional program suffix (e.g. `make PROG_SUFFIX=-alt` will create binary nexcache-server-alt).
# Passed from `make test` as environment variable NEXCACHE_PROG_SUFFIX.
set ::NEXCACHE_PROG_SUFFIX [expr {
    [info exists ::env(NEXCACHE_PROG_SUFFIX)] ? $::env(NEXCACHE_PROG_SUFFIX) : ""
}]

# Helper to build absolute paths
proc nexcache_bin_absolute_path {name} {
    set full_name "${name}${::NEXCACHE_PROG_SUFFIX}"
    return [file join $::NEXCACHE_BIN_DIR $full_name]
}

set ::NEXCACHE_SERVER_BIN    [nexcache_bin_absolute_path "nexcache-server"]
set ::NEXCACHE_CLI_BIN       [nexcache_bin_absolute_path "nexcache-cli"]
set ::NEXCACHE_BENCHMARK_BIN [nexcache_bin_absolute_path "nexcache-benchmark"]
set ::NEXCACHE_CHECK_AOF_BIN [nexcache_bin_absolute_path "nexcache-check-aof"]
set ::NEXCACHE_CHECK_RDB_BIN [nexcache_bin_absolute_path "nexcache-check-rdb"]
set ::NEXCACHE_SENTINEL_BIN  [nexcache_bin_absolute_path "nexcache-sentinel"]

# TLS module path: in CMake builds it's in lib/, in Make builds it's in src/
if {[info exists ::env(NEXCACHE_BIN_DIR)]} {
    # CMake build: lib/ is sibling to bin/
    set ::NEXCACHE_TLS_MODULE [file join [file dirname $::NEXCACHE_BIN_DIR] "lib" "nexcache-tls${::NEXCACHE_PROG_SUFFIX}.so"]
} else {
    set ::NEXCACHE_TLS_MODULE "[pwd]/src/nexcache-tls${::NEXCACHE_PROG_SUFFIX}.so"
}

if {![file executable $::NEXCACHE_SERVER_BIN]} {
    error "Binary not found or not executable: $::NEXCACHE_SERVER_BIN"
}