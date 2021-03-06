#include <string>
#include <iostream>
#include <rocksdb/db.h>
#include <rocksdb/convenience.h>
#include <rocksdb/write_batch.h>
#include <rocksdb/cache.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/cache.h>
#include <rocksdb/comparator.h>
#include <rocksdb/env.h>
#include <rocksdb/options.h>
#include <rocksdb/table.h>
#include <rocksdb/c.h>
#include <emscripten/bind.h>
#include <algorithm>

using namespace rocksdb;
using namespace emscripten;

class Rocksdb {
  private:
    DB* db;
  public:
    Rocksdb() {}
    Status Open(const Options& options, const std::string& name) {
      return DB::Open(options, name, &this->db);
    }
    Options * getOptions() {
        return new Options();
    }
  ~Rocksdb() {}
};

EMSCRIPTEN_BINDINGS(rocksdbM) {
//   value_object<rocksdb_options_t>("rocksdb_options_t")
//     .field("rep", &rocksdb_options_t::rep);

  class_<Status>("Status")
    .constructor()
    .function("ok", &Status::ok);
  class_<Rocksdb>("Rocksdb")
    .constructor<>()
    .function("Open", &Rocksdb::Open)
    .function("getOptions", &Rocksdb::getOptions, allow_raw_pointers());

  function("rocksdb_options_create", &rocksdb_options_create, allow_raw_pointers());



}

