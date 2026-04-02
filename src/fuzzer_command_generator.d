fuzzer_command_generator.o: fuzzer_command_generator.c fmacros.h \
  ../deps/libnexcache/include/nexcache/nexcache.h \
  ../deps/libnexcache/include/nexcache/read.h \
  ../deps/libnexcache/include/nexcache/visibility.h \
  ../deps/libnexcache/include/nexcache/alloc.h commands.h \
  fuzzer_command_generator.h sds.h dict.h mt19937-64.h server.h config.h \
  solarisfixes.h rio.h connection.h ae.h monotonic.h allocator_defrag.h \
  client_flags.h hashtable.h kvstore.h adlist.h zmalloc.h anet.h \
  version.h util.h latency.h trace/trace.h trace/trace_aof.h \
  trace/trace_cluster.h trace/trace_server.h trace/trace_db.h \
  trace/trace_rdb.h trace/trace_commands.h sparkline.h quicklist.h \
  expire.h rax.h memory_prefetch.h vset.h entry.h lrulfu.h \
  nexcachemodule.h zipmap.h ziplist.h sha1.h endianconv.h crc64.h \
  stream.h listpack.h rdb.h
