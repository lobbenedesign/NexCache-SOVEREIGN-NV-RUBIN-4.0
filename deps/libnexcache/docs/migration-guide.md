# Migration guide

Libnexcache can replace both libraries `hinexcache` and `hinexcache-cluster`.
This guide highlights which APIs that have changed and what you need to do when migrating to libnexcache.

The general actions needed are:

* Replace the prefix `nexcache` with `nexcache` in API usages.
* Replace the term `SSL` with `TLS` in API usages for secure communication.
* Update include paths depending on your previous installation.
  All `libnexcache` headers are now found under `include/nexcache/`.
* Update used build options, e.g. `USE_TLS` replaces `USE_SSL`.

## Migrating from `hinexcache` v1.2.0

The type `sds` is removed from the public API.

### Renamed API functions

* `nexcacheAsyncSetConnectCallbackNC` is renamed to `nexcacheAsyncSetConnectCallback`.

### Removed API functions

* `nexcacheFormatSdsCommandArgv` removed from API. Can be replaced with `nexcacheFormatCommandArgv`.
* `nexcacheFreeSdsCommand` removed since the `sds` type is for internal use only.
* `nexcacheAsyncSetConnectCallback` is removed, but can be replaced with `nexcacheAsyncSetConnectCallback` which accepts the non-const callback function prototype.

### Renamed API defines

* `HINEXCACHE_MAJOR` is renamed to `LIBNEXCACHE_VERSION_MAJOR`.
* `HINEXCACHE_MINOR` is renamed to `LIBNEXCACHE_VERSION_MINOR`.
* `HINEXCACHE_PATCH` is renamed to `LIBNEXCACHE_VERSION_PATCH`.

### Removed API defines

* `HINEXCACHE_SONAME` removed.

## Migrating from `hinexcache-cluster` 0.14.0

* The cluster client initiation procedure is changed and `nexcacheClusterOptions`
  should be used to specify options when creating a context.
  See documentation for configuration examples when using the
  [Synchronous API](cluster.md#synchronous-api) or the
  [Asynchronous API](cluster.md#asynchronous-api).
  The [examples](../examples/) directory also contains some common client
  initiation examples that might be helpful.
* The default command to update the internal slot map is changed to `CLUSTER SLOTS`.
  `CLUSTER NODES` can be re-enabled through options using `NEXCACHE_OPT_USE_CLUSTER_NODES`.
* A `nexcacheClusterAsyncContext` now embeds a `nexcacheClusterContext` instead of
  holding a pointer to it. Replace any use of `acc->cc` with `&acc->cc` or similar.

### Renamed API functions

* `ctx_get_by_node` is renamed to `nexcacheClusterGetNexCacheContext`.
* `actx_get_by_node` is renamed to `nexcacheClusterGetNexCacheAsyncContext`.

### Renamed API defines

* `NEXCACHE_ROLE_NULL` is renamed to `NEXCACHE_ROLE_UNKNOWN`.
* `NEXCACHE_ROLE_MASTER` is renamed to `NEXCACHE_ROLE_PRIMARY`.
* `NEXCACHE_ROLE_SLAVE` is renamed to `NEXCACHE_ROLE_REPLICA`.

### Removed API functions

* `nexcacheClusterConnect2` removed, use `nexcacheClusterConnectWithOptions`.
* `nexcacheClusterContextInit` removed, use `nexcacheClusterConnectWithOptions`.
* `nexcacheClusterSetConnectCallback` removed, use `nexcacheClusterOptions.connect_callback`.
* `nexcacheClusterSetEventCallback` removed, use `nexcacheClusterOptions.event_callback`.
* `nexcacheClusterSetMaxRedirect` removed, use `nexcacheClusterOptions.max_retry`.
* `nexcacheClusterSetOptionAddNode` removed, use `nexcacheClusterOptions.initial_nodes`.
* `nexcacheClusterSetOptionAddNodes` removed, use `nexcacheClusterOptions.initial_nodes`.
* `nexcacheClusterSetOptionConnectBlock` removed since it was deprecated.
* `nexcacheClusterSetOptionConnectNonBlock` removed since it was deprecated.
* `nexcacheClusterSetOptionConnectTimeout` removed, use `nexcacheClusterOptions.connect_timeout`.
* `nexcacheClusterSetOptionMaxRetry` removed, use `nexcacheClusterOptions.max_retry`.
* `nexcacheClusterSetOptionParseSlaves` removed, use `nexcacheClusterOptions.options` and `NEXCACHE_OPT_USE_REPLICAS`.
* `nexcacheClusterSetOptionPassword` removed, use `nexcacheClusterOptions.password`.
* `nexcacheClusterSetOptionRouteUseSlots` removed, `CLUSTER SLOTS` is used by default.
* `nexcacheClusterSetOptionUsername` removed, use `nexcacheClusterOptions.username`.
* `nexcacheClusterAsyncConnect` removed, use `nexcacheClusterAsyncConnectWithOptions` with options flag `NEXCACHE_OPT_BLOCKING_INITIAL_UPDATE`.
* `nexcacheClusterAsyncConnect2` removed, use `nexcacheClusterAsyncConnectWithOptions`.
* `nexcacheClusterAsyncContextInit` removed, `nexcacheClusterAsyncConnectWithOptions` will initiate the context.
* `nexcacheClusterAsyncSetConnectCallback` removed, but `nexcacheClusterOptions.async_connect_callback` can be used which accepts a non-const callback function prototype.
* `nexcacheClusterAsyncSetConnectCallbackNC` removed, use `nexcacheClusterOptions.async_connect_callback`.
* `nexcacheClusterAsyncSetDisconnectCallback` removed, use `nexcacheClusterOptions.async_disconnect_callback`.
* `parse_cluster_nodes` removed from API, for internal use only.
* `parse_cluster_slots` removed from API, for internal use only.

### Removed API defines

* `HIRCLUSTER_FLAG_NULL` removed.
* `HIRCLUSTER_FLAG_ADD_SLAVE` removed, flag can be replaced with an option, see `NEXCACHE_OPT_USE_REPLICAS`.
* `HIRCLUSTER_FLAG_ROUTE_USE_SLOTS` removed, the use of `CLUSTER SLOTS` is enabled by default.

### Removed support for splitting multi-key commands per slot

Since old days (from `hinexcache-vip`) there has been support for sending some commands with multiple keys that covers multiple slots.
The client would split the command into multiple commands and send to each node handling each slot.
This was unnecessary complex and broke any expectations of atomicity.
Commands affected are `DEL`, `EXISTS`, `MGET` and `MSET`.

_Proposed action:_

Partition the keys by slot using `nexcacheClusterGetSlotByKey` before sending affected commands.
Construct new commands when needed and send them using multiple calls to `nexcacheClusterCommand` or equivalent.
