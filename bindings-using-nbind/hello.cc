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

using namespace rocksdb;

#include "nbind/nbind.h"

NBIND_CLASS(Rocksdb) {
    construct<>();
    method(Open);
}

NBIND_CLASS(Options) {
    construct<>();
    construct<DBOptions&, ColumnFamilyOptions&>();
}

NBIND_CLASS(Status) {
    construct<>();
    method(ok);
}

class Rocksdb {
  private:
    DB* db;
  public:
    Rocksdb() {}
    Status Open(const Options& options, const std::string& name) {
      return DB::Open(options, name, &this->db);
    }
  ~Rocksdb() {}
};

NBIND_CLASS(Rocksdb) {
    construct<>();
    method(Open);
}



