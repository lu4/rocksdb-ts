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
        Status Open(std::string options, const std::string& name) {
            Options opt;
            std::string options_string =
                "create_if_missing=true;max_open_files=1000;"
                "block_based_table_factory={block_size=4096}";

            GetDBOptionsFromString(opt, options_string, &opt);

            return DB::Open(opt, name, &this->db);
        }
    ~Rocksdb() {}
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
