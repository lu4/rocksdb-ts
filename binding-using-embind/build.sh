#!/usr/bin/env bash
ROCKSDB_INCLUDE=/workspaces/rocksdb-ts/deps/rocksdb/rocksdb/include
ROCKSDB_INCLUDE2=/workspaces/rocksdb-ts/deps/rocksdb/rocksdb
em++ --bind -I$ROCKSDB_INCLUDE -I$ROCKSDB_INCLUDE2 -s WASM=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s EXPORTED_FUNCTIONS="['rocksdb_options_create']" -s EXTRA_EXPORTED_RUNTIME_METHODS='["cwrap"]' -s EMULATE_FUNCTION_POINTER_CASTS=1 -s ASSERTIONS=1 -s LINKABLE=1 -s DEMANGLE_SUPPORT=1 -s EXPORT_ALL=1 -O3 rocksdb.cpp ../deps/rocksdb/rocksdb/db/c.cc
