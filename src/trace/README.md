## Introduction

This directory contains the implementation of tracing using [LTTng](https://lttng.org/) (Linux Trace Toolkit Next Generation).

## LTTng Installation

To install LTTng on your Linux system, follow the instructions provided in the [LTTng documentation](https://lttng.org/download/)

> Dependency LTTNG version is greater than 2.12.

### Install from package manager

#### [Ubuntu](https://lttng.org/docs/v2.13/#doc-ubuntu)

LTTng 2.13 is available on Ubuntu 22.04 LTS *Jammy Jellyfish*, Ubuntu 23.04 *Lunar Lobster*, and Ubuntu 23.10 *Mantic Minotaur*. For previous supported releases of Ubuntu, [use the LTTng Stable 2.13 PPA](https://lttng.org/docs/v2.13/#doc-ubuntu-ppa).

To install LTTng 2.13 on Ubuntu 22.04 LTS *Jammy Jellyfish*:

1. Install the main LTTng 2.13 packages:
   ```
   apt-get install lttng-tools
   apt-get install lttng-modules-dkms
   apt-get install liblttng-ust-dev
   ```

#### [Debian](https://lttng.org/docs/v2.13/#doc-debian)

To install LTTng 2.13 on Debian 12 *bookworm*:

1. Install the main LTTng 2.13 packages:
   ```
   apt install lttng-modules-dkms
   apt install liblttng-ust-dev
   apt install lttng-tools
   ```

## LTTng QuickStart

LTTng is an open source tracing framework for Linux that provides highly efficient and low-overhead tracing capabilities. It allows developers to trace both kernel and user-space applications.

Building NexCache with LTTng support:

```
USE_LTTNG=yes make
```

Enable lttng trace events dynamically:
```
~# lttng destroy nexcache
~# lttng create nexcache
~# lttng enable-event -u 'nexcache_server:*'
~# lttng enable-event -u 'nexcache_commands:*'
~# lttng track -u -p `pidof nexcache-server`
~# lttng start
~# lttng stop
~# lttng view
```

Examples (a client run 'SET', another run 'keys'):
```
...
[15:30:19.334467738] (+0.000001222) xxx nexcache_commands:command_call: { cpu_id = 15 }, { name = "set", duration = 0 }
[15:30:19.334469105] (+0.000001367) xxx nexcache_commands:command_call: { cpu_id = 15 }, { name = "set", duration = 1 }
[15:30:19.334470327] (+0.000001222) xxx nexcache_commands:command_call: { cpu_id = 15 }, { name = "set", duration = 0 }
[15:30:19.369348485] (+0.034878158) xxx nexcache_commands:command_call: { cpu_id = 15 }, { name = "keys", duration = 34874 }
[15:30:19.369698322] (+0.000349837) xxx nexcache_commands:command_call: { cpu_id = 15 }, { name = "set", duration = 4 }
[15:30:19.369702327] (+0.000004005) xxx nexcache_commands:command_call: { cpu_id = 15 }, { name = "set", duration = 2 }
[15:30:19.369704098] (+0.000001771) xxx nexcache_commands:command_call: { cpu_id = 15 }, { name = "set", duration = 1 }
[15:30:19.369705884] (+0.000001786) xxx nexcache_commands:command_call: { cpu_id = 15 }, { name = "set", duration = 0 }
...
```

Then we can use another script to analyze topN slow commands and other system
level events.

About performance overhead (nexcache-benchmark -t get -n 1000000 --threads 4):
1> no lttng builtin: 285632.69 requests per second
2> lttng builtin, no trace: 285551.09 requests per second (almost 0 overhead)
3> lttng builtin, trace commands: 266595.59 requests per second (about ~6.6 overhead)

Generally nexcache-server would not run in full utilization, the overhead is acceptable.

## Supported Events

| event                    | provider        |
|--------------------------|-----------------|
| command_call             | nexcache_commands |
| eventloop                | nexcache_server   |
| eventloop_cron           | nexcache_server   |
| while_blocked_cron       | nexcache_server   |
| module_acquire_gil       | nexcache_server   |
| command_unblocking       | nexcache_server   |
| fast_command             | nexcache_server   |
| command                  | nexcache_server   |
| expire_del               | nexcache_db       |
| active_defrag_cycle      | nexcache_db       |
| eviction_del             | nexcache_db       |
| eviction_lazyfree        | nexcache_db       |
| eviction_cycle           | nexcache_db       |
| expire_cycle             | nexcache_db       |
| expire_cycle_fields      | nexcache_db       |
| expire_cycle_keys        | nexcache_db       |
| fork                     | nexcache_cluster  |
| cluster_config_open      | nexcache_cluster  |
| cluster_config_write     | nexcache_cluster  |
| cluster_config_fsync     | nexcache_cluster  |
| cluster_config_rename    | nexcache_cluster  |
| cluster_config_dir_fsync | nexcache_cluster  |
| cluster_config_close     | nexcache_cluster  |
| cluster_config_unlink    | nexcache_cluster  |
| fork                     | nexcache_rdb      |
| rdb_unlink_temp_file     | nexcache_rdb      |
| fork                     | nexcache_aof      |
| aof_write_pending_fsync  | nexcache_aof      |
| aof_write_active_child   | nexcache_aof      |
| aof_write_alone          | nexcache_aof      |
| aof_write                | nexcache_aof      |
| aof_fsync_always         | nexcache_aof      |
| aof_fstat                | nexcache_aof      |
| aof_rename               | nexcache_aof      |
| aof_flush                | nexcache_aof      |
