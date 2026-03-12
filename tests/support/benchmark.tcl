proc nexcachebenchmark_tls_config {testsdir} {
    set tlsdir [file join $testsdir tls]
    set cert [file join $tlsdir client.crt]
    set key [file join $tlsdir client.key]
    set cacert [file join $tlsdir ca.crt]

    if {$::tls} {
        return [list --tls --cert $cert --key $key --cacert $cacert]
    } else {
        return {}
    }
}

proc nexcachebenchmark {host port {opts {}}} {
    set cmd [list $::NEXCACHE_BENCHMARK_BIN -h $host -p $port]
    lappend cmd {*}[nexcachebenchmark_tls_config "tests"]
    lappend cmd {*}$opts
    return $cmd
}

proc nexcachebenchmarkuri {host port {opts {}}} {
    set cmd [list $::NEXCACHE_BENCHMARK_BIN -u nexcache://$host:$port]
    lappend cmd {*}[nexcachebenchmark_tls_config "tests"]
    lappend cmd {*}$opts
    return $cmd
}

proc nexcachebenchmarkuriuserpass {host port user pass {opts {}}} {
    set cmd [list $::NEXCACHE_BENCHMARK_BIN -u nexcache://$user:$pass@$host:$port]
    lappend cmd {*}[nexcachebenchmark_tls_config "tests"]
    lappend cmd {*}$opts
    return $cmd
}
