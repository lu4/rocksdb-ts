# rocksdb-ts

This is a complete rewrite of https://github.com/Level/rocksdb with aim is to provide a complete RocksDB interface for NodeJS without LevelUP or LevelDown constraints.

We're planning to provide two kinds of APIs in this repository, the low-level api and a high-level api.

Low-level api is aimed to become as lean as possible abstraction over RocksDB native interface operating with maximum performance in mind.

Also we plan to cover two modes of operation for low-level api sync and async. Sync mode api is executed on the main thread and thus all of it's operations are blocked by native calls to RocksDB C++ interface.
Async api wraps native calls to RocksDB interface into node's NAPI Promises essentially causing all of the calls to become executed in the background thread(s) managed by NAPI.
Async mode of operation will support a dedicated namespace organized into list of methods with "async" postfix all of which will return promise.

Both sync and async namespaces are interchangeable so you can combine sync and async operations in a way you find suitable for yourself
