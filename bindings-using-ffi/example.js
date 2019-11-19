const ffi = require('ffi');
const ref = require("ref");

const RocksDbOptions = ref.types.void;
const RocksDbOptionsPtr = ref.refType(RocksDbOptions);
const RocksDb = ref.types.void;
const RocksDbPtr = ref.refType(RocksDb);

const rocksdb_lib = ffi.Library('librocksdb', {
    "rocksdb_options_create": [ RocksDbOptionsPtr, [] ],
    "rocksdb_options_set_create_if_missing": [ref.types.void, [RocksDbOptionsPtr, ref.types.uchar]],
    "rocksdb_open": [RocksDbPtr, [RocksDbOptionsPtr, 'string', ref.refType('string')]]
});

const err = Buffer.from("");
const opt = rocksdb_lib.rocksdb_options_create();
rocksdb_lib.rocksdb_options_set_create_if_missing(opt, 1);
const db = rocksdb_lib.rocksdb_open(opt, "./test14.db", err);
