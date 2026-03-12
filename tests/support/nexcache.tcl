# Tcl client library - used by the server test
# Copyright (c) 2009-2014 NexCache Contributors.
# Released under the BSD license like NexCache itself
#
# Example usage:
#
# set r [nexcache 127.0.0.1 6379]
# $r lpush mylist foo
# $r lpush mylist bar
# $r lrange mylist 0 -1
# $r close
#
# Non blocking usage example:
#
# proc handlePong {r type reply} {
#     puts "PONG $type '$reply'"
#     if {$reply ne "PONG"} {
#         $r ping [list handlePong]
#     }
# }
#
# set r [nexcache]
# $r blocking 0
# $r get fo [list handlePong]
#
# vwait forever

package provide nexcache 0.1

source [file join [file dirname [info script]] "response_transformers.tcl"]

namespace eval nexcache {}
set ::nexcache::id 0
array set ::nexcache::fd {}
array set ::nexcache::addr {}
array set ::nexcache::blocking {}
array set ::nexcache::deferred {}
array set ::nexcache::readraw {}
array set ::nexcache::attributes {} ;# Holds the RESP3 attributes from the last call
array set ::nexcache::reconnect {}
array set ::nexcache::tls {}
array set ::nexcache::callback {}
array set ::nexcache::state {} ;# State in non-blocking reply reading
array set ::nexcache::statestack {} ;# Stack of states, for nested mbulks
array set ::nexcache::curr_argv {} ;# Remember the current argv, to be used in response_transformers.tcl
array set ::nexcache::testing_resp3 {} ;# Indicating if the current client is using RESP3 (only if the test is trying to test RESP3 specific behavior. It won't be on in case of force_resp3)

set ::force_resp3 0
set ::log_req_res 0

proc nexcache {{server 127.0.0.1} {port 6379} {defer 0} {tls 0} {tlsoptions {}} {readraw 0}} {
    if {$tls} {
        package require tls
        ::tls::init \
            -cafile "$::tlsdir/ca.crt" \
            -certfile "$::tlsdir/client.crt" \
            -keyfile "$::tlsdir/client.key" \
            {*}$tlsoptions
        set fd [::tls::socket $server $port]
    } else {
        set fd [socket $server $port]
    }
    fconfigure $fd -translation binary
    set id [incr ::nexcache::id]
    set ::nexcache::fd($id) $fd
    set ::nexcache::addr($id) [list $server $port]
    set ::nexcache::blocking($id) 1
    set ::nexcache::deferred($id) $defer
    set ::nexcache::readraw($id) $readraw
    set ::nexcache::reconnect($id) 0
    set ::nexcache::curr_argv($id) 0
    set ::nexcache::testing_resp3($id) 0
    set ::nexcache::tls($id) $tls
    ::nexcache::nexcache_reset_state $id
    interp alias {} ::nexcache::nexcacheHandle$id {} ::nexcache::__dispatch__ $id
}

# On recent versions of tcl-tls/OpenSSL, reading from a dropped connection
# results with an error we need to catch and mimic the old behavior.
proc ::nexcache::nexcache_safe_read {fd len} {
    if {$len == -1} {
        set err [catch {set val [read $fd]} msg]
    } else {
        set err [catch {set val [read $fd $len]} msg]
    }
    if {!$err} {
        return $val
    }
    if {[string match "*connection abort*" $msg]} {
        return {}
    }
    error $msg
}

proc ::nexcache::nexcache_safe_gets {fd} {
    if {[catch {set val [gets $fd]} msg]} {
        if {[string match "*connection abort*" $msg]} {
            return {}
        }
        error $msg
    }
    return $val
}

# This is a wrapper to the actual dispatching procedure that handles
# reconnection if needed.
proc ::nexcache::__dispatch__ {id method args} {
    set errorcode [catch {::nexcache::__dispatch__raw__ $id $method $args} retval]
    if {$errorcode && $::nexcache::reconnect($id) && $::nexcache::fd($id) eq {}} {
        # Try again if the connection was lost.
        # FIXME: we don't re-select the previously selected DB, nor we check
        # if we are inside a transaction that needs to be re-issued from
        # scratch.
        set errorcode [catch {::nexcache::__dispatch__raw__ $id $method $args} retval]
    }
    return -code $errorcode $retval
}

proc ::nexcache::__dispatch__raw__ {id method argv} {
    set fd $::nexcache::fd($id)

    # Reconnect the link if needed.
    if {$fd eq {} && $method ne {close}} {
        lassign $::nexcache::addr($id) host port
        if {$::nexcache::tls($id)} {
            set ::nexcache::fd($id) [::tls::socket $host $port]
        } else {
            set ::nexcache::fd($id) [socket $host $port]
        }
        fconfigure $::nexcache::fd($id) -translation binary
        set fd $::nexcache::fd($id)
    }

    # Transform HELLO 2 to HELLO 3 if force_resp3
    # All set the connection var testing_resp3 in case of HELLO 3
    if {[llength $argv] > 0 && [string compare -nocase $method "HELLO"] == 0} {
        if {[lindex $argv 0] == 3} {
            set ::nexcache::testing_resp3($id) 1
        } else {
            set ::nexcache::testing_resp3($id) 0
            if {$::force_resp3} {
                # If we are in force_resp3 we run HELLO 3 instead of HELLO 2
                lset argv 0 3
            }
        }
    }

    set blocking $::nexcache::blocking($id)
    set deferred $::nexcache::deferred($id)
    if {$blocking == 0} {
        if {[llength $argv] == 0} {
            error "Please provide a callback in non-blocking mode"
        }
        set callback [lindex $argv end]
        set argv [lrange $argv 0 end-1]
    }
    if {[info command ::nexcache::__method__$method] eq {}} {
        catch {unset ::nexcache::attributes($id)}
        set cmd "*[expr {[llength $argv]+1}]\r\n"
        append cmd "$[string length $method]\r\n$method\r\n"
        foreach a $argv {
            append cmd "$[string length $a]\r\n$a\r\n"
        }
        ::nexcache::nexcache_write $fd $cmd
        if {[catch {flush $fd}]} {
            catch {close $fd}
            set ::nexcache::fd($id) {}
            return -code error "I/O error reading reply"
        }

        set ::nexcache::curr_argv($id) [concat $method $argv]
        if {!$deferred} {
            if {$blocking} {
                ::nexcache::nexcache_read_reply $id $fd
            } else {
                # Every well formed reply read will pop an element from this
                # list and use it as a callback. So pipelining is supported
                # in non blocking mode.
                lappend ::nexcache::callback($id) $callback
                fileevent $fd readable [list ::nexcache::nexcache_readable $fd $id]
            }
        }
    } else {
        uplevel 1 [list ::nexcache::__method__$method $id $fd] $argv
    }
}

proc ::nexcache::__method__blocking {id fd val} {
    set ::nexcache::blocking($id) $val
    fconfigure $fd -blocking $val
}

proc ::nexcache::__method__reconnect {id fd val} {
    set ::nexcache::reconnect($id) $val
}

proc ::nexcache::__method__read {id fd} {
    ::nexcache::nexcache_read_reply $id $fd
}

proc ::nexcache::__method__rawread {id fd {len -1}} {
    return [nexcache_safe_read $fd $len]
}

proc ::nexcache::__method__write {id fd buf} {
    ::nexcache::nexcache_write $fd $buf
}

proc ::nexcache::__method__flush {id fd} {
    flush $fd
}

proc ::nexcache::__method__close {id fd} {
    catch {close $fd}
    catch {unset ::nexcache::fd($id)}
    catch {unset ::nexcache::addr($id)}
    catch {unset ::nexcache::blocking($id)}
    catch {unset ::nexcache::deferred($id)}
    catch {unset ::nexcache::readraw($id)}
    catch {unset ::nexcache::attributes($id)}
    catch {unset ::nexcache::reconnect($id)}
    catch {unset ::nexcache::tls($id)}
    catch {unset ::nexcache::state($id)}
    catch {unset ::nexcache::statestack($id)}
    catch {unset ::nexcache::callback($id)}
    catch {unset ::nexcache::curr_argv($id)}
    catch {unset ::nexcache::testing_resp3($id)}
    catch {interp alias {} ::nexcache::nexcacheHandle$id {}}
}

proc ::nexcache::__method__channel {id fd} {
    return $fd
}

proc ::nexcache::__method__deferred {id fd val} {
    set ::nexcache::deferred($id) $val
}

proc ::nexcache::__method__readraw {id fd val} {
    set ::nexcache::readraw($id) $val
}

proc ::nexcache::__method__readingraw {id fd} {
    return $::nexcache::readraw($id)
}

proc ::nexcache::__method__attributes {id fd} {
    set _ $::nexcache::attributes($id)
}

proc ::nexcache::nexcache_write {fd buf} {
    puts -nonewline $fd $buf
}

proc ::nexcache::nexcache_writenl {fd buf} {
    nexcache_write $fd $buf
    nexcache_write $fd "\r\n"
    flush $fd
}

proc ::nexcache::nexcache_readnl {fd len} {
    set buf [nexcache_safe_read $fd $len]
    nexcache_safe_read $fd 2 ; # discard CR LF
    return $buf
}

proc ::nexcache::nexcache_bulk_read {fd} {
    set count [nexcache_read_line $fd]
    if {$count == -1} return {}
    set buf [nexcache_readnl $fd $count]
    return $buf
}

proc ::nexcache::nexcache_multi_bulk_read {id fd} {
    set count [nexcache_read_line $fd]
    if {$count == -1} return {}
    set l {}
    set err {}
    for {set i 0} {$i < $count} {incr i} {
        if {[catch {
            lappend l [nexcache_read_reply_logic $id $fd]
        } e] && $err eq {}} {
            set err $e
        }
    }
    if {$err ne {}} {return -code error $err}
    return $l
}

proc ::nexcache::nexcache_read_map {id fd} {
    set count [nexcache_read_line $fd]
    if {$count == -1} return {}
    set d {}
    set err {}
    for {set i 0} {$i < $count} {incr i} {
        if {[catch {
            set k [nexcache_read_reply_logic $id $fd] ; # key
            set v [nexcache_read_reply_logic $id $fd] ; # value
            dict set d $k $v
        } e] && $err eq {}} {
            set err $e
        }
    }
    if {$err ne {}} {return -code error $err}
    return $d
}

proc ::nexcache::nexcache_read_line fd {
    string trim [nexcache_safe_gets $fd]
}

proc ::nexcache::nexcache_read_null fd {
    nexcache_safe_gets $fd
    return {}
}

proc ::nexcache::nexcache_read_bool fd {
    set v [nexcache_read_line $fd]
    if {$v == "t"} {return 1}
    if {$v == "f"} {return 0}
    return -code error "Bad protocol, '$v' as bool type"
}

proc ::nexcache::nexcache_read_double {id fd} {
    set v [nexcache_read_line $fd]
    # unlike many other DTs, there is a textual difference between double and a string with the same value,
    # so we need to transform to double if we are testing RESP3 (i.e. some tests check that a
    # double reply is "1.0" and not "1")
    if {[should_transform_to_resp2 $id]} {
        return $v
    } else {
        return [expr {double($v)}]
    }
}

proc ::nexcache::nexcache_read_verbatim_str fd {
    set v [nexcache_bulk_read $fd]
    # strip the first 4 chars ("txt:")
    return [string range $v 4 end]
}

proc ::nexcache::nexcache_read_reply_logic {id fd} {
    if {$::nexcache::readraw($id)} {
        return [nexcache_read_line $fd]
    }

    while {1} {
        set type [nexcache_safe_read $fd 1]
        switch -exact -- $type {
            _ {return [nexcache_read_null $fd]}
            : -
            ( -
            + {return [nexcache_read_line $fd]}
            , {return [nexcache_read_double $id $fd]}
            # {return [nexcache_read_bool $fd]}
            = {return [nexcache_read_verbatim_str $fd]}
            - {return -code error [nexcache_read_line $fd]}
            $ {return [nexcache_bulk_read $fd]}
            > -
            ~ -
            * {return [nexcache_multi_bulk_read $id $fd]}
            % {return [nexcache_read_map $id $fd]}
            | {
                set attrib [nexcache_read_map $id $fd]
                set ::nexcache::attributes($id) $attrib
                continue
            }
            default {
                if {$type eq {}} {
                    catch {close $fd}
                    set ::nexcache::fd($id) {}
                    return -code error "I/O error reading reply"
                }
                return -code error "Bad protocol, '$type' as reply type byte"
            }
        }
    }
}

proc ::nexcache::nexcache_read_reply {id fd} {
    set response [nexcache_read_reply_logic $id $fd]
    ::response_transformers::transform_response_if_needed $id $::nexcache::curr_argv($id) $response
}

proc ::nexcache::nexcache_reset_state id {
    set ::nexcache::state($id) [dict create buf {} mbulk -1 bulk -1 reply {}]
    set ::nexcache::statestack($id) {}
}

proc ::nexcache::nexcache_call_callback {id type reply} {
    set cb [lindex $::nexcache::callback($id) 0]
    set ::nexcache::callback($id) [lrange $::nexcache::callback($id) 1 end]
    uplevel #0 $cb [list ::nexcache::nexcacheHandle$id $type $reply]
    ::nexcache::nexcache_reset_state $id
}

# Read a reply in non-blocking mode.
proc ::nexcache::nexcache_readable {fd id} {
    if {[eof $fd]} {
        nexcache_call_callback $id eof {}
        ::nexcache::__method__close $id $fd
        return
    }
    if {[dict get $::nexcache::state($id) bulk] == -1} {
        set line [gets $fd]
        if {$line eq {}} return ;# No complete line available, return
        switch -exact -- [string index $line 0] {
            : -
            + {nexcache_call_callback $id reply [string range $line 1 end-1]}
            - {nexcache_call_callback $id err [string range $line 1 end-1]}
            ( {nexcache_call_callback $id reply [string range $line 1 end-1]}
            $ {
                dict set ::nexcache::state($id) bulk \
                    [expr [string range $line 1 end-1]+2]
                if {[dict get $::nexcache::state($id) bulk] == 1} {
                    # We got a $-1, hack the state to play well with this.
                    dict set ::nexcache::state($id) bulk 2
                    dict set ::nexcache::state($id) buf "\r\n"
                    ::nexcache::nexcache_readable $fd $id
                }
            }
            * {
                dict set ::nexcache::state($id) mbulk [string range $line 1 end-1]
                # Handle *-1
                if {[dict get $::nexcache::state($id) mbulk] == -1} {
                    nexcache_call_callback $id reply {}
                }
            }
            default {
                nexcache_call_callback $id err \
                    "Bad protocol, $type as reply type byte"
            }
        }
    } else {
        set totlen [dict get $::nexcache::state($id) bulk]
        set buflen [string length [dict get $::nexcache::state($id) buf]]
        set toread [expr {$totlen-$buflen}]
        set data [read $fd $toread]
        set nread [string length $data]
        dict append ::nexcache::state($id) buf $data
        # Check if we read a complete bulk reply
        if {[string length [dict get $::nexcache::state($id) buf]] ==
            [dict get $::nexcache::state($id) bulk]} {
            if {[dict get $::nexcache::state($id) mbulk] == -1} {
                nexcache_call_callback $id reply \
                    [string range [dict get $::nexcache::state($id) buf] 0 end-2]
            } else {
                dict with ::nexcache::state($id) {
                    lappend reply [string range $buf 0 end-2]
                    incr mbulk -1
                    set bulk -1
                }
                if {[dict get $::nexcache::state($id) mbulk] == 0} {
                    nexcache_call_callback $id reply \
                        [dict get $::nexcache::state($id) reply]
                }
            }
        }
    }
}

# when forcing resp3 some tests that rely on resp2 can fail, so we have to translate the resp3 response to resp2
proc ::nexcache::should_transform_to_resp2 {id} {
    return [expr {$::force_resp3 && !$::nexcache::testing_resp3($id)}]
}
