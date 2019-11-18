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

class Rocksdb {
  private:
    DB* db;
  public:
    Rocksdb() {}
    Status Open(const Options& options, const std::string& name) {
        try {
            return DB::Open(options, name, &this->db);
        } catch(const std::exception& e) {
            std::cout << "Error: " << e.what();
        }
    }
    ~Rocksdb() {
        delete this->db;
    }
};

#include <genepi/genepi.h>


GENEPI_CLASS( Options )
{
   GENEPI_CONSTRUCTOR();
   GENEPI_CONSTRUCTOR(DBOptions&, ColumnFamilyOptions&);
}

GENEPI_CLASS( Status )
{
    GENEPI_CONSTRUCTOR();
    GENEPI_METHOD(ok);
}

GENEPI_CLASS( Rocksdb )
{
    GENEPI_CONSTRUCTOR();
    GENEPI_METHOD(Open);
}
