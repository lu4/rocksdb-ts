#include <napi-macros.h>
#include <node_api.h>
// #include <assert.h>

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

#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP

bool logging_enabled = false;

#define VERSION "(v0.0.1)"
#define LOCATION_S1(x) #x
#define LOCATION_S2(x) LOCATION_S1(x)
#define LOCATION __FILE__ ":" LOCATION_S2(__LINE__) VERSION

#define NAPI_RETURN_UNDEFINED() \
    return 0;

#define NAPI_BAD_STATUS_IS_FATAL(call, file, line, reason)                                        \
    if ((call) != napi_ok)                                                                        \
    {                                                                                             \
        napi_fatal_error(#file ":" #line " " #call, NAPI_AUTO_LENGTH, #reason, NAPI_AUTO_LENGTH); \
    }

#define FROM_EXTERNAL_ARGUMENT(index, name) \
    name = nullptr;                         \
    NAPI_STATUS_THROWS(napi_get_value_external(env, argv[index], (void **)&name));

// This macro is quite unsafe since buffer does not return null terminated string,
// so using anywhere in the code must be accompaniated with great care and attention
// #define CHAR_PTR_FROM_BUFFER_ARGUMENT(index, name) \
//     char *name = 0;                                        \
//     size_t name##_size = 0;                                \
//     napi_value name##_napi = argv[index];                  \
//     NAPI_STATUS_THROWS(napi_get_buffer_info(env, argv[index], (void **)&name, &name##_size));

#define STRING_FROM_BUFFER_ARGUMENT(index, name)                                                      \
    char *name##_chars = 0;                                                                           \
    size_t name##_size = 0;                                                                           \
    napi_value name##_napi = argv[index];                                                             \
    NAPI_STATUS_THROWS(napi_get_buffer_info(env, argv[index], (void **)&name##_chars, &name##_size)); \
    std::string name(name##_chars, name##_size);

// This macro is quite unsafe since buffer does not return null terminated string,
// so using anywhere in the code must be accompaniated with great care and attention
// #define CHAR_PTR_FROM_STRING_ARGUMENT_OF_TYPE(type, index, name)      \
//     size_t name##_size;                                                       \
//     napi_get_value_string_##type(env, argv[index], nullptr, 0, &name##_size); \
//     char *name##_buf = new char[name##_size];                                 \
//     NAPI_STATUS_THROWS(napi_get_value_string_##type(env, argv[index], name##_buf, name##_size, &name##_size));

#define SLICE_FROM_STRING_ARGUMENT(type, index, name)                                                          \
    size_t name##_size;                                                                                        \
    napi_get_value_string_##type(env, argv[index], nullptr, 0, &name##_size);                                  \
    name##_size++;                                                                                             \
    char *name##_buf = new char[name##_size];                                                                  \
    NAPI_STATUS_THROWS(napi_get_value_string_##type(env, argv[index], name##_buf, name##_size, &name##_size)); \
    rocksdb::Slice name(name##_buf, name##_size);

#define SLICE_FROM_STRING_ARGUMENT_RELEASE(name) \
    delete[] name##_buf

#define SLICE_FROM_BUFFER_ARGUMENT(index, name)                                                     \
    char *name##_buf = 0;                                                                           \
    size_t name##_size = 0;                                                                         \
    NAPI_STATUS_THROWS(napi_get_buffer_info(env, argv[index], (void **)&name##_buf, &name##_size)); \
    rocksdb::Slice name(name##_buf, name##_size);

#define SLICE_PTR_FROM_BUFFER_ARGUMENT(index, name)                                                 \
    char *name##_buf = 0;                                                                           \
    size_t name##_size = 0;                                                                         \
    NAPI_STATUS_THROWS(napi_get_buffer_info(env, argv[index], (void **)&name##_buf, &name##_size)); \
    rocksdb::Slice *name = new rocksdb::Slice(name##_buf, name##_size);

#define ROCKSDB_STATUS_THROWS(call)                                              \
    {                                                                            \
        rocksdb::Status status_macro_var = call;                                 \
                                                                                 \
        if (!status_macro_var.ok())                                              \
        {                                                                        \
            napi_throw_error(env, nullptr, status_macro_var.ToString().c_str()); \
        }                                                                        \
    }

class Promise
{
private:
    bool success;
    bool executing;

    napi_env env;
    napi_value promise;
    napi_deferred deferred;
    napi_async_work async_work;

protected:
    virtual bool execute() = 0;
    virtual napi_value rejection(napi_env env) = 0;
    virtual napi_value resolution(napi_env env) = 0;

private:
    static void Begin(napi_env env, void *data)
    {
        Promise *self = (Promise *)(data);
        self->executing = true;
        self->success = self->execute();
        self->executing = false;
    }

    static void Finish(napi_env env, napi_status status, void *data)
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::Finish (started)";

        Promise *self = (Promise *)(data);

        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::Begin (declared self)";

        if (self->success)
        {
            // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::Finish (successful resolution)";
            NAPI_STATUS_THROWS_VOID(napi_resolve_deferred(env, self->deferred, self->resolution(env)));
        }
        else
        {
            // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::Finish (failed resolution)";
            NAPI_STATUS_THROWS_VOID(napi_reject_deferred(env, self->deferred, self->rejection(env)));
        }
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::Finish (finished)";
    }

public:
    Promise(napi_env env, const char *id)
        : success(false), executing(false), env(env)
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::ctor (started)";
        NAPI_STATUS_THROWS_VOID(napi_create_promise(env, &deferred, &promise));
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::ctor (napi_create_promise)";

        napi_value id_napi_value;
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::ctor (id_napi_value)";
        NAPI_STATUS_THROWS_VOID(napi_create_string_utf8(env, id, NAPI_AUTO_LENGTH, &id_napi_value));
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::ctor (napi_create_async_work)";
        NAPI_STATUS_THROWS_VOID(napi_create_async_work(env, nullptr, id_napi_value, Promise::Begin, Promise::Finish, this, &async_work));
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::ctor (finished)";
    }

    void start()
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::start (started)";
        NAPI_STATUS_THROWS_VOID(napi_queue_async_work(env, async_work));
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::start (finished)";
    }

    napi_value get() const
    {
        return this->promise;
    }

    ~Promise()
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::dtor (started)";
        NAPI_STATUS_THROWS_VOID(napi_delete_async_work(env, async_work));

        if (this->executing)
        {
            // LOG_IF(logging_enabled, FATAL) << LOCATION << " Promise::dtor (executing)";

            napi_fatal_error(LOCATION, NAPI_AUTO_LENGTH, "Promise was disposed during execution, terminating to prevent stack corruption", NAPI_AUTO_LENGTH);
        }
        // LOG_IF(logging_enabled, INFO) << LOCATION << " Promise::dtor (finished)";
    }
};

class ReferenceContainer
{
public:
    napi_ref reference;
};

/**
 * Runs when a rocksdb::WriteOptions instance is garbage collected.
 */
static void rocksdb_slice_finalize(napi_env env, void *data, void *hint)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_finalize (started)";
    if (data)
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_finalize (disposing)";
        delete (rocksdb::Slice *)data;
    }
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_finalize (finished)";
}

NAPI_METHOD(rocksdb_slice)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_slice (started)";

    NAPI_ARGV(1);

    SLICE_PTR_FROM_BUFFER_ARGUMENT(0, slice);

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_external(env, slice, rocksdb_slice_finalize, nullptr, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_slice (finished)";
    return result;
}

/**
 * Runs when a rocksdb::WriteOptions instance is garbage collected.
 */
static void rocksdb_write_options_finalize(napi_env env, void *data, void *hint)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_finalize (started)";
    if (data)
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_finalize (disposing)";
        delete (rocksdb::WriteOptions *)data;
    }
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_finalize (finished)";
}

NAPI_METHOD(rocksdb_write_options_init)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_init (started)";

    rocksdb::WriteOptions *options = new rocksdb::WriteOptions();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_external(env, options, rocksdb_write_options_finalize, nullptr, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_init (finished)";
    return result;
}

NAPI_METHOD(rocksdb_write_options_get_sync)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_get_sync (started)";

    NAPI_ARGV(1);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->sync, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_get_sync (finished)";
    return result;
}

NAPI_METHOD(rocksdb_write_options_get_disableWAL)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_get_disableWAL (started)";

    NAPI_ARGV(1);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->disableWAL, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_get_disableWAL (finished)";
    return result;
}

NAPI_METHOD(rocksdb_write_options_get_ignore_missing_column_families)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_get_ignore_missing_column_families (started)";

    NAPI_ARGV(1);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->ignore_missing_column_families, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_get_ignore_missing_column_families (finished)";
    return result;
}

NAPI_METHOD(rocksdb_write_options_get_no_slowdown)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_get_no_slowdown (started)";

    NAPI_ARGV(1);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->no_slowdown, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_get_no_slowdown (finished)";
    return result;
}

NAPI_METHOD(rocksdb_write_options_get_low_pri)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_get_low_pri (started)";

    NAPI_ARGV(1);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->low_pri, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_get_low_pri (finished)";
    return result;
}

NAPI_METHOD(rocksdb_write_options_set_timestamp)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_timestamp (started)";

    NAPI_ARGV(2);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);
    rocksdb::Slice *FROM_EXTERNAL_ARGUMENT(1, timestamp);

    options->timestamp = timestamp;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_timestamp (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_write_options_reset_timestamp)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_timestamp (started)";

    NAPI_ARGV(2);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value undefined;
    NAPI_STATUS_THROWS(napi_get_undefined(env, &undefined));

    options->timestamp = nullptr;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_timestamp (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_write_options_set_sync)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_sync (started)";

    NAPI_ARGV(2);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->sync = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_sync (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_write_options_set_disableWAL)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_disableWAL (started)";

    NAPI_ARGV(2);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->disableWAL = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_disableWAL (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_write_options_set_ignore_missing_column_families)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_ignore_missing_column_families (started)";

    NAPI_ARGV(2);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->ignore_missing_column_families = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_ignore_missing_column_families (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_write_options_set_no_slowdown)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_no_slowdown (started)";

    NAPI_ARGV(2);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->no_slowdown = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_no_slowdown (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_write_options_set_low_pri)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_low_pri (started)";

    NAPI_ARGV(2);

    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->low_pri = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_write_options_set_low_pri (finished)";
    NAPI_RETURN_UNDEFINED();
}

/**
 * Runs when a rocksdb::Options instance is garbage collected.
 */
static void rocksdb_options_finalize(napi_env env, void *data, void *hint)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_options_finalize (started)";
    if (data)
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_options_finalize (disposing)";
        delete (rocksdb::Options *)data;
    }
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_options_finalize (finished)";
}

NAPI_METHOD(rocksdb_options_init)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_options_init (started)";

    rocksdb::Options *options = new rocksdb::Options();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_external(env, options, rocksdb_options_finalize, nullptr, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_options_init (finished)";
    return result;
}

NAPI_METHOD(rocksdb_options_init_from_buffer)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_options_init_from_buffer (started)";

    NAPI_ARGV(1);

    STRING_FROM_BUFFER_ARGUMENT(0, options_string);

    rocksdb::Options *options = new rocksdb::Options();

    ROCKSDB_STATUS_THROWS(GetDBOptionsFromString(*options, options_string, options));

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_external(env, options, rocksdb_options_finalize, nullptr, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_options_init_from_buffer (finished)";
    return result;
}

/**
 * Runs when a rocksdb::DB instance is garbage collected.
 */
static void rocksdb_finalize(napi_env env, void *db_ptr, void *reference_container_ptr)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_finalize (started)";

    napi_status napi_delete_reference_status = napi_status::napi_ok;

    if (reference_container_ptr)
    {
        ReferenceContainer *reference_container = (ReferenceContainer *)reference_container_ptr;

        napi_delete_reference_status = napi_delete_reference(env, reference_container->reference);

        delete reference_container;
    }

    rocksdb::Status db_close_status = rocksdb::Status::OK();

    if (db_ptr)
    {
        rocksdb::DB *db = (rocksdb::DB *)db_ptr;
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_finalize (closing db)";
        db_close_status = db->Close();

        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_finalize (disposing db)";
        delete db;
    }

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_finalize (checking operation napi_delete_reference_status code)";
    NAPI_STATUS_THROWS_VOID(napi_delete_reference_status);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_finalize (checking operation db_close_status code " << db_close_status.code() << ", subcode " << db_close_status.subcode() << ")";
    ROCKSDB_STATUS_THROWS(db_close_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_finalize (finished)";
}

/**
 * Opens a database.
 */
NAPI_METHOD(rocksdb_open)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_open (started)";

    NAPI_ARGV(2);

    STRING_FROM_BUFFER_ARGUMENT(0, location);

    rocksdb::Options *FROM_EXTERNAL_ARGUMENT(1, options);

    rocksdb::DB *db;
    ROCKSDB_STATUS_THROWS(rocksdb::DB::Open(*options, location, &db));

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_open (started, " << db << ")";

    ReferenceContainer *reference_container = new ReferenceContainer();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_external(env, db, rocksdb_finalize, reference_container, &result));

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_open (creating reference)";

    NAPI_STATUS_THROWS(napi_create_reference(env, result, 1, &reference_container->reference));

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_open (finished)";

    return result;
}

/**
 * Closes a database.
 */
NAPI_METHOD(rocksdb_close)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_close (started)";

    NAPI_ARGV(1);

    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);

    ROCKSDB_STATUS_THROWS(db->Close());

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_close (finished)";
    NAPI_RETURN_UNDEFINED();
}

/**
 * Puts key / value pair into a database.
 */
NAPI_METHOD(rocksdb_put)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_put (started)";

    NAPI_ARGV(4);

    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    SLICE_FROM_BUFFER_ARGUMENT(1, k);
    SLICE_FROM_BUFFER_ARGUMENT(2, v);
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(3, options);

    ROCKSDB_STATUS_THROWS(db->Put(*options, k, v));

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_put (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_put_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_put_utf8 (started)";
    NAPI_ARGV(4);
    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    SLICE_FROM_STRING_ARGUMENT(utf8, 1, k);
    SLICE_FROM_STRING_ARGUMENT(utf8, 2, v);
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(3, options);
    rocksdb::Status put_status = db->Put(*options, k, v);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(k);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(v);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_put_utf8 (checking put status)";
    ROCKSDB_STATUS_THROWS(put_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_put_utf8 (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_put_latin1)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_put_latin1 (started)";
    NAPI_ARGV(4);
    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    SLICE_FROM_STRING_ARGUMENT(latin1, 1, k);
    SLICE_FROM_STRING_ARGUMENT(latin1, 2, v);
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(3, options);
    rocksdb::Status put_status = db->Put(*options, k, v);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(k);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(v);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_put_latin1 (checking put status)";
    ROCKSDB_STATUS_THROWS(put_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_put_latin1 (finished)";
    NAPI_RETURN_UNDEFINED();
}

/**
 * Gets key / value pair from a database.
 */
NAPI_METHOD(rocksdb_get)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get (started)";

    NAPI_ARGV(3);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get (getting db argument)";
    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get (getting db argument success, value = " << db << ")";

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get (getting key argument)";
    SLICE_FROM_BUFFER_ARGUMENT(1, k);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get (getting options argument)";
    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(2, options);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get (declaring value variable)";
    rocksdb::PinnableSlice v;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get (getting value from database)";
    rocksdb::Status get_status = db->Get(*options, db->DefaultColumnFamily(), k, &v);

    napi_value result = nullptr;
    if (get_status.ok())
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get (wrapping value with js wrapper)";
        NAPI_STATUS_THROWS(napi_create_buffer_copy(env, v.size(), (void *)v.data(), nullptr, &result));
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get (finished)";
    }
    else if (get_status.IsNotFound())
    {
        NAPI_STATUS_THROWS(napi_get_undefined(env, &result));
    }
    else
    {
        ROCKSDB_STATUS_THROWS(get_status);
    }

    return result;
}

NAPI_METHOD(rocksdb_get_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_utf8 (started)";
    NAPI_ARGV(3);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_utf8 (getting db argument)";
    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_utf8 (getting key argument)";
    SLICE_FROM_STRING_ARGUMENT(utf8, 1, k);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_utf8 (getting options argument)";
    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(2, options);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_utf8 (declaring value variable)";
    rocksdb::PinnableSlice v;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_utf8 (getting value from database)";
    rocksdb::Status get_status = db->Get(*options, db->DefaultColumnFamily(), k, &v);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_utf8 (release key slice)";
    SLICE_FROM_STRING_ARGUMENT_RELEASE(k);

    napi_value result = nullptr;
    if (get_status.ok())
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_utf8 (wrapping value with js wrapper)";
        NAPI_STATUS_THROWS(napi_create_string_utf8(env, v.data(), v.size(), &result));
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_utf8 (finished)";
    }
    else if (get_status.IsNotFound())
    {
        NAPI_STATUS_THROWS(napi_get_undefined(env, &result));
    }
    else
    {
        ROCKSDB_STATUS_THROWS(get_status);
    }

    return result;
}

NAPI_METHOD(rocksdb_get_latin1)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_latin1 (started)";
    NAPI_ARGV(3);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_latin1 (getting db argument)";
    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_latin1 (getting key argument)";
    SLICE_FROM_STRING_ARGUMENT(latin1, 1, k);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_latin1 (getting options argument)";
    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(2, options);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_latin1 (declaring value variable)";
    rocksdb::PinnableSlice v;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_latin1 (getting value from database)";
    rocksdb::Status get_status = db->Get(*options, db->DefaultColumnFamily(), k, &v);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_latin1 (release key slice)";
    SLICE_FROM_STRING_ARGUMENT_RELEASE(k);

    napi_value result = nullptr;
    if (get_status.ok())
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_latin1 (wrapping value with js wrapper)";
        NAPI_STATUS_THROWS(napi_create_string_latin1(env, v.data(), v.size(), &result));
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_get_latin1 (finished)";
    }
    else if (get_status.IsNotFound())
    {
        NAPI_STATUS_THROWS(napi_get_undefined(env, &result));
    }
    else
    {
        ROCKSDB_STATUS_THROWS(get_status);
    }

    return result;
}

NAPI_METHOD(rocksdb_delete)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete (started)";

    NAPI_ARGV(3);

    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    SLICE_FROM_BUFFER_ARGUMENT(1, k);
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(2, options);

    rocksdb::Status delete_status = db->Delete(*options, k);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete (checking status)";
    ROCKSDB_STATUS_THROWS(delete_status);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_delete_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_utf8 (started)";
    NAPI_ARGV(3);
    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    SLICE_FROM_STRING_ARGUMENT(utf8, 1, k);
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(2, options);
    rocksdb::Status delete_status = db->Delete(*options, k);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(k);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_utf8 (checking status)";
    ROCKSDB_STATUS_THROWS(delete_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_utf8 (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_delete_latin1)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_latin1 (started)";
    NAPI_ARGV(3);
    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    SLICE_FROM_STRING_ARGUMENT(latin1, 1, k);
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(2, options);
    rocksdb::Status delete_status = db->Delete(*options, k);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(k);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_latin1 (checking status)";
    ROCKSDB_STATUS_THROWS(delete_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_latin1 (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_delete_range)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_range (started)";

    NAPI_ARGV(4);

    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    SLICE_FROM_BUFFER_ARGUMENT(1, from);
    SLICE_FROM_BUFFER_ARGUMENT(2, till);
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(3, options);

    rocksdb::Status delete_status = db->DeleteRange(*options, db->DefaultColumnFamily(), from, till);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_range (checking status)";
    ROCKSDB_STATUS_THROWS(delete_status);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_range (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_delete_range_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_range_utf8 (started)";
    NAPI_ARGV(4);
    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    SLICE_FROM_STRING_ARGUMENT(utf8, 1, from);
    SLICE_FROM_STRING_ARGUMENT(utf8, 2, till);
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(3, options);
    rocksdb::Status delete_status = db->DeleteRange(*options, db->DefaultColumnFamily(), from, till);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(from);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(till);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_range_utf8 (checking status)";
    ROCKSDB_STATUS_THROWS(delete_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_range_utf8 (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_delete_range_latin1)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_range_latin1 (started)";
    NAPI_ARGV(4);
    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    SLICE_FROM_STRING_ARGUMENT(latin1, 1, from);
    SLICE_FROM_STRING_ARGUMENT(latin1, 2, till);
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(3, options);
    rocksdb::Status delete_status = db->DeleteRange(*options, db->DefaultColumnFamily(), from, till);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(from);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(till);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_range_latin1 (checking status)";
    ROCKSDB_STATUS_THROWS(delete_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_delete_range_latin1 (finished)";
    NAPI_RETURN_UNDEFINED();
}

/**
 * Destroys a database.
 */
NAPI_METHOD(rocksdb_destroy)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_destroy (started)";

    NAPI_ARGV(2);
    STRING_FROM_BUFFER_ARGUMENT(0, location);
    rocksdb::Options *FROM_EXTERNAL_ARGUMENT(1, options);

    ROCKSDB_STATUS_THROWS(rocksdb::DestroyDB(location, *options));

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_destroy (finished)";
    NAPI_RETURN_UNDEFINED();
}

/**
 * Repairs a database.
 */
NAPI_METHOD(rocksdb_repair)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_repair (started)";

    NAPI_ARGV(2);
    STRING_FROM_BUFFER_ARGUMENT(0, location);
    rocksdb::Options *FROM_EXTERNAL_ARGUMENT(1, options);

    ROCKSDB_STATUS_THROWS(rocksdb::RepairDB(location, *options));

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_repair (finished)";
    NAPI_RETURN_UNDEFINED();
}

/**
 * Runs when a rocksdb::Options instance is garbage collected.
 */
static void rocksdb_iterator_finalize(napi_env env, void *data, void *hint)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_finalize (started)";
    if (data)
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_finalize (disposing)";
        delete (rocksdb::Iterator *)data;
    }
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_finalize (finished)";
}

/**
 * Gets iterator from a database.
 */
NAPI_METHOD(rocksdb_iterator_init)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_init (started)";

    NAPI_ARGV(2);
    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(1, options);

    rocksdb::Iterator *iterator = db->NewIterator(*options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_external(env, iterator, rocksdb_iterator_finalize, nullptr, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_init (finished)";
    return result;
}

/**
 * Runs Seek operation on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_seek)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek (started)";

    NAPI_ARGV(2);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    SLICE_FROM_BUFFER_ARGUMENT(1, key);

    iterator->Seek(key);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek (finished)";
    return result;
}

NAPI_METHOD(rocksdb_iterator_seek_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_utf8 (started)";
    NAPI_ARGV(2);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);
    SLICE_FROM_STRING_ARGUMENT(utf8, 1, key);
    iterator->Seek(key);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(key);
    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_utf8 (finished)";
    return result;
}

NAPI_METHOD(rocksdb_iterator_seek_latin1)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_latin1 (started)";
    NAPI_ARGV(2);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);
    SLICE_FROM_STRING_ARGUMENT(latin1, 1, key);
    iterator->Seek(key);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(key);
    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_latin1 (finished)";
    return result;
}

/**
 * Runs SeekForPrev operation on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_seek_for_prev)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_for_prev (started)";

    NAPI_ARGV(2);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    SLICE_FROM_BUFFER_ARGUMENT(1, key);

    iterator->SeekForPrev(key);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_for_prev (finished)";
    return result;
}

NAPI_METHOD(rocksdb_iterator_seek_for_prev_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_for_prev_utf8 (started)";
    NAPI_ARGV(2);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);
    SLICE_FROM_STRING_ARGUMENT(utf8, 1, key);
    iterator->SeekForPrev(key);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(key);
    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_for_prev_utf8 (finished)";
    return result;
}

NAPI_METHOD(rocksdb_iterator_seek_for_prev_latin1)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_for_prev_latin1 (started)";
    NAPI_ARGV(2);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);
    SLICE_FROM_STRING_ARGUMENT(latin1, 1, key);
    iterator->SeekForPrev(key);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(key);
    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_for_prev_latin1 (finished)";
    return result;
}

/**
 * Runs SeekToFirst operation on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_seek_for_first)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_for_first (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    iterator->SeekToFirst();

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_for_first (finished)";
    return result;
}

/**
 * Runs SeekToLast operation on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_seek_for_last)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_for_last (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    iterator->SeekToLast();

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_seek_for_last (finished)";
    return result;
}

/**
 * Runs Reset operation on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_reset)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_reset (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    iterator->Reset();

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_reset (finished)";
    return result;
}

/**
 * Checks iterator "Valid" status on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_valid)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_valid (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_valid (finished)";
    return result;
}

/**
 * Checks iterator "Valid" status on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_refresh)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_refresh (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    ROCKSDB_STATUS_THROWS(iterator->Refresh())

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_refresh (finished)";
    return result;
}

/**
 * Checks iterator "Valid" status on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_next)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_next (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    iterator->Next();

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_next (finished)";
    return result;
}

/**
 * Checks iterator "Valid" status on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_prev)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_prev (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    iterator->Prev();

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, iterator->Valid(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_prev (finished)";
    return result;
}

NAPI_METHOD(rocksdb_iterator_key)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_key (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    rocksdb::Slice k = iterator->key();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_buffer_copy(env, k.size(), k.data(), nullptr, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_key (finished)";
    return result;
}

NAPI_METHOD(rocksdb_iterator_key_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_key_utf8 (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    rocksdb::Slice k = iterator->key();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_string_utf8(env, k.data(), k.size(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_key_utf8 (finished)";
    return result;
}

NAPI_METHOD(rocksdb_iterator_key_latin1)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_key_latin1 (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    rocksdb::Slice k = iterator->key();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_string_latin1(env, k.data(), k.size(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_key_latin1 (finished)";
    return result;
}

/**
 * Checks iterator "Valid" status on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_value)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_value (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    rocksdb::Slice v = iterator->value();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_buffer_copy(env, v.size(), v.data(), nullptr, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_value (finished)";
    return result;
}

NAPI_METHOD(rocksdb_iterator_value_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_value_utf8 (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    rocksdb::Slice v = iterator->value();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_string_utf8(env, v.data(), v.size(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_value_utf8 (finished)";
    return result;
}

NAPI_METHOD(rocksdb_iterator_value_latin1)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_value_latin1 (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    rocksdb::Slice v = iterator->value();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_string_latin1(env, v.data(), v.size(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_value_latin1 (finished)";
    return result;
}

/**
 * Checks iterator "Valid" status on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_key_value_pair)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_key_value_pair (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    rocksdb::Slice k = iterator->key();
    rocksdb::Slice v = iterator->value();

    void *buffer;

    napi_value k_r;
    NAPI_STATUS_THROWS(napi_create_buffer_copy(env, k.size(), k.data(), &buffer, &k_r));

    napi_value v_r;
    NAPI_STATUS_THROWS(napi_create_buffer_copy(env, v.size(), v.data(), &buffer, &v_r));

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_array_with_length(env, 2, &result));
    NAPI_STATUS_THROWS(napi_set_element(env, result, 0, k_r));
    NAPI_STATUS_THROWS(napi_set_element(env, result, 1, v_r));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_key_value_pair (finished)";
    return result;
}

/**
 * Checks iterator "Valid" status on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_key_value_pair_of_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_key_value_pair_of_utf8 (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    rocksdb::Slice k = iterator->key();
    rocksdb::Slice v = iterator->value();

    void *buffer;

    napi_value k_r;
    NAPI_STATUS_THROWS(napi_create_string_utf8(env, k.data(), k.size(), &k_r));

    napi_value v_r;
    NAPI_STATUS_THROWS(napi_create_string_utf8(env, v.data(), v.size(), &v_r));

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_array_with_length(env, 2, &result));
    NAPI_STATUS_THROWS(napi_set_element(env, result, 0, k_r));
    NAPI_STATUS_THROWS(napi_set_element(env, result, 1, v_r));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_key_value_pair_of_utf8 (finished)";
    return result;
}

/**
 * Checks iterator "Valid" status on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_status_code)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_status_code (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_int32(env, iterator->status().code(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_status_code (finished)";
    return result;
}

/**
 * Checks iterator "Valid" status on a database iterator.
 */
NAPI_METHOD(rocksdb_iterator_status_subcode)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_status_subcode (started)";

    NAPI_ARGV(1);
    rocksdb::Iterator *FROM_EXTERNAL_ARGUMENT(0, iterator);

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_int32(env, iterator->status().subcode(), &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_iterator_status_subcode (finished)";
    return result;
}

/**
 * Runs when a rocksdb::Options instance is garbage collected.
 */
static void rocksdb_batch_finalize(napi_env env, void *data, void *hint)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_finalize (started)";
    if (data)
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_finalize (disposing)";
        delete (rocksdb::WriteBatch *)data;
    }
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_finalize (finished)";
}

/**
 * Gets iterator from a database.
 */
NAPI_METHOD(rocksdb_batch_init)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_init (started)";

    rocksdb::WriteBatch *batch = new rocksdb::WriteBatch();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_external(env, batch, rocksdb_batch_finalize, nullptr, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_init (finished)";
    return result;
}

/**
 * Gets iterator from a database.
 */
NAPI_METHOD(rocksdb_batch_init_ex)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_init_ex (started)";

    NAPI_ARGV(2);

    bool loossless;

    uint64_t reserved_bytes;
    NAPI_STATUS_THROWS(napi_get_value_bigint_uint64(env, argv[0], &reserved_bytes, &loossless));

    uint64_t max_bytes;
    NAPI_STATUS_THROWS(napi_get_value_bigint_uint64(env, argv[1], &max_bytes, &loossless));

    rocksdb::WriteBatch *batch = new rocksdb::WriteBatch((size_t)reserved_bytes, (size_t)max_bytes);

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_external(env, batch, rocksdb_batch_finalize, nullptr, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_init_ex (finished)";
    return result;
}

/**
 * Gets iterator from a database.
 */
NAPI_METHOD(rocksdb_batch_put)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_put (started)";

    NAPI_ARGV(3);

    rocksdb::WriteBatch *FROM_EXTERNAL_ARGUMENT(0, batch);

    SLICE_FROM_BUFFER_ARGUMENT(1, key);
    SLICE_FROM_BUFFER_ARGUMENT(2, value);

    rocksdb::Status put_status = batch->Put(key, value);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_put (checking status)";
    ROCKSDB_STATUS_THROWS(put_status);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_put (finished)";
    NAPI_RETURN_UNDEFINED();
}

// TODO: Provide support for utf16
NAPI_METHOD(rocksdb_batch_put_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_put_utf8 (started)";
    NAPI_ARGV(3);
    rocksdb::WriteBatch *FROM_EXTERNAL_ARGUMENT(0, batch);
    SLICE_FROM_STRING_ARGUMENT(utf8, 1, key);
    SLICE_FROM_STRING_ARGUMENT(utf8, 2, value);
    rocksdb::Status put_status = batch->Put(key, value);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(key);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(value);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_put_utf8 (checking status)";
    ROCKSDB_STATUS_THROWS(put_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_put_utf8 (finished)";
    NAPI_RETURN_UNDEFINED();
}
NAPI_METHOD(rocksdb_batch_put_latin1)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_put_latin1 (started)";
    NAPI_ARGV(3);
    rocksdb::WriteBatch *FROM_EXTERNAL_ARGUMENT(0, batch);
    SLICE_FROM_STRING_ARGUMENT(latin1, 1, key);
    SLICE_FROM_STRING_ARGUMENT(latin1, 2, value);
    rocksdb::Status put_status = batch->Put(key, value);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(key);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(value);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_put_latin1 (checking status)";
    ROCKSDB_STATUS_THROWS(put_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_put_latin1 (finished)";
    NAPI_RETURN_UNDEFINED();
}

/**
 * Gets iterator from a database.
 */
NAPI_METHOD(rocksdb_batch_delete)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_delete (started)";
    NAPI_ARGV(2);
    rocksdb::WriteBatch *FROM_EXTERNAL_ARGUMENT(0, batch);
    SLICE_FROM_BUFFER_ARGUMENT(1, key);
    rocksdb::Status delete_status = batch->Delete(key);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_delete (checking status)";
    ROCKSDB_STATUS_THROWS(delete_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_delete (finished)";
    NAPI_RETURN_UNDEFINED();
}

// TODO: Provide support for utf16
NAPI_METHOD(rocksdb_batch_delete_utf8)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_delete_utf8 (started)";
    NAPI_ARGV(2);
    rocksdb::WriteBatch *FROM_EXTERNAL_ARGUMENT(0, batch);
    SLICE_FROM_STRING_ARGUMENT(utf8, 1, key);
    rocksdb::Status delete_status = batch->Delete(key);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(key);
    ROCKSDB_STATUS_THROWS(delete_status);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_delete_utf8 (finished)";
    NAPI_RETURN_UNDEFINED();
}
NAPI_METHOD(rocksdb_batch_delete_latin1)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_delete_latin1 (started)";
    NAPI_ARGV(2);
    rocksdb::WriteBatch *FROM_EXTERNAL_ARGUMENT(0, batch);
    SLICE_FROM_STRING_ARGUMENT(latin1, 1, key);
    rocksdb::Status delete_status = batch->Delete(key);
    ROCKSDB_STATUS_THROWS(delete_status);
    SLICE_FROM_STRING_ARGUMENT_RELEASE(key);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_delete_latin1 (finished)";
    NAPI_RETURN_UNDEFINED();
}
/**
 * Gets iterator from a database.
 */
NAPI_METHOD(rocksdb_batch_clear)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_clear (started)";

    NAPI_ARGV(1);

    rocksdb::WriteBatch *FROM_EXTERNAL_ARGUMENT(0, batch);

    batch->Clear();

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_clear (finished)";
    NAPI_RETURN_UNDEFINED();
}

/**
 * Gets iterator from a database.
 */
NAPI_METHOD(rocksdb_batch_write)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write (started)";

    NAPI_ARGV(3);

    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    rocksdb::WriteBatch *FROM_EXTERNAL_ARGUMENT(1, batch);
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(2, options);

    ROCKSDB_STATUS_THROWS(db->Write(*options, batch));

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write (finished)";
    NAPI_RETURN_UNDEFINED();
}

/**
 * Worker class for async batch write operation
 */
class BatchWritePromise final : public Promise
{
private:
    rocksdb::Status status;

    rocksdb::DB *db;
    rocksdb::WriteBatch *batch;
    rocksdb::WriteOptions *options;

protected:
    virtual napi_value rejection(napi_env env) override
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " BatchWritePromise::rejection (started)";
        napi_value result;
        std::string error = status.ToString();
        napi_create_string_utf8(env, error.c_str(), error.size(), &result);

        // LOG_IF(logging_enabled, INFO) << LOCATION << " BatchWritePromise::rejection (finished)";
        return result;
    }
    virtual napi_value resolution(napi_env env) override
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " BatchWritePromise::resolution (started)";
        napi_value result;
        napi_get_undefined(env, &result);
        // LOG_IF(logging_enabled, INFO) << LOCATION << " BatchWritePromise::resolution (finished)";
        return result;
    }

    bool execute() override
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " BatchWritePromise::execute (started, finished)";
        return (status = db->Write(*options, batch)).ok();
    }

public:
    // Promise posesses no ownership over provided arguments, so no destructor for these values here
    BatchWritePromise(napi_env env, rocksdb::DB *db, rocksdb::WriteBatch *batch, rocksdb::WriteOptions *options)
        : Promise(env, "BatchWritePromise")
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " BatchWritePromise::ctor (start)";
        this->db = db;
        // LOG_IF(logging_enabled, INFO) << LOCATION << " BatchWritePromise::ctor (assigned db)";
        this->batch = batch;
        // LOG_IF(logging_enabled, INFO) << LOCATION << " BatchWritePromise::ctor (assigned batch)";
        this->options = options;
        // LOG_IF(logging_enabled, INFO) << LOCATION << " BatchWritePromise::ctor (assigned options, finish)";
    }
};

/**
 * Runs when a rocksdb::Options instance is garbage collected.
 */
static void rocksdb_batch_write_async_finalize(napi_env env, void *data, void *hint)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write_async_finalize (started)";
    if (data)
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write_async_finalize (disposing)";
        delete (BatchWritePromise *)data;
    }
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write_async_finalize (finished)";
}

/**
 * Gets iterator from a database.
 */
NAPI_METHOD(rocksdb_batch_write_async)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write_async (started)";

    NAPI_ARGV(3);

    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write_async (declared db)";
    rocksdb::WriteBatch *FROM_EXTERNAL_ARGUMENT(1, batch);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write_async (declared batch)";
    rocksdb::WriteOptions *FROM_EXTERNAL_ARGUMENT(2, options);
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write_async (declared options)";

    BatchWritePromise *promise = new BatchWritePromise(env, db, batch, options);

    // When promise gets garbage collected we want to garbage collect the native promise instance
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write_async (adding finalizer)";
    napi_add_finalizer(env, promise->get(), promise, rocksdb_batch_write_async_finalize, nullptr, nullptr);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write_async (starting promise)";
    promise->start();

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_batch_write_async (finished)";
    return promise->get();
}

/**
 * Runs when a rocksdb::WriteOptions instance is garbage collected.
 */
static void rocksdb_snapshot_finalize(napi_env env, void *snapshot_ptr, void *db_ptr)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_snapshot_finalize (started)";
    if (snapshot_ptr && db_ptr)
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_snapshot_finalize (disposing)";
        static_cast<rocksdb::DB *>(db_ptr)->ReleaseSnapshot(static_cast<rocksdb::Snapshot *>(snapshot_ptr));
    }
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_snapshot_finalize (finished)";
}

/**
 * Gets iterator from a database.
 */
NAPI_METHOD(rocksdb_snapshot_init)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_snapshot_init (started)";

    NAPI_ARGV(1);

    rocksdb::DB *FROM_EXTERNAL_ARGUMENT(0, db);
    rocksdb::Snapshot *snapshot = (rocksdb::Snapshot *)(db->GetSnapshot());

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_external(env, snapshot, rocksdb_snapshot_finalize, db, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_snapshot_init (finished)";
    return result;
}

/**
 * Runs when a rocksdb::WriteOptions instance is garbage collected.
 */
static void rocksdb_read_options_finalize(napi_env env, void *wrapper_ptr, void *not_used)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_finalize (started)";
    if (wrapper_ptr)
    {
        // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_finalize (disposing)";
        delete static_cast<rocksdb::ReadOptions *>(wrapper_ptr);
    }
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_finalize (finished)";
}

/**
 * Gets iterator from a database.
 */
NAPI_METHOD(rocksdb_read_options_init)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_init (started)";

    rocksdb::ReadOptions *options = new rocksdb::ReadOptions();

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_external(env, options, rocksdb_read_options_finalize, nullptr, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_init (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_set_snapshot)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_snapshot (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);
    rocksdb::Snapshot *FROM_EXTERNAL_ARGUMENT(1, snapshot);

    options->snapshot = snapshot;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_snapshot (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_reset_snapshot)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_snapshot (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value undefined;
    NAPI_STATUS_THROWS(napi_get_undefined(env, &undefined));

    options->snapshot = nullptr;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_snapshot (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_timestamp)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_timestamp (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);
    rocksdb::Slice *FROM_EXTERNAL_ARGUMENT(1, timestamp);

    options->timestamp = timestamp;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_timestamp (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_reset_timestamp)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_timestamp (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value undefined;
    NAPI_STATUS_THROWS(napi_get_undefined(env, &undefined));

    options->timestamp = nullptr;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_timestamp (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_iterate_lower_bound)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_iterate_lower_bound (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);
    rocksdb::Slice *FROM_EXTERNAL_ARGUMENT(1, iterate_lower_bound);

    options->iterate_lower_bound = iterate_lower_bound;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_iterate_lower_bound (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_reset_iterate_lower_bound)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_reset_iterate_lower_bound (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    options->iterate_lower_bound = nullptr;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_reset_iterate_lower_bound (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_iterate_upper_bound)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_iterate_upper_bound (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);
    rocksdb::Slice *FROM_EXTERNAL_ARGUMENT(1, iterate_upper_bound);

    options->iterate_upper_bound = iterate_upper_bound;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_iterate_upper_bound (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_reset_iterate_upper_bound)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_reset_iterate_upper_bound (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    options->iterate_upper_bound = nullptr;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_reset_iterate_upper_bound (finished)";
    NAPI_RETURN_UNDEFINED();
}

// -------------------------------

NAPI_METHOD(rocksdb_read_options_get_verify_checksums)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_verify_checksums (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->verify_checksums, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_verify_checksums (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_fill_cache)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_fill_cache (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->fill_cache, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_fill_cache (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_tailing)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_tailing (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->tailing, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_tailing (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_managed)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_managed (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->managed, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_managed (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_total_order_seek)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_total_order_seek (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->total_order_seek, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_total_order_seek (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_prefix_same_as_start)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_prefix_same_as_start (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->prefix_same_as_start, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_prefix_same_as_start (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_pin_data)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_pin_data (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->pin_data, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_pin_data (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_background_purge_on_iterator_cleanup)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_background_purge_on_iterator_cleanup (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->background_purge_on_iterator_cleanup, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_background_purge_on_iterator_cleanup (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_ignore_range_deletions)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_ignore_range_deletions (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_get_boolean(env, options->ignore_range_deletions, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_ignore_range_deletions (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_max_skippable_internal_keys)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_max_skippable_internal_keys (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_bigint_uint64(env, options->max_skippable_internal_keys, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_max_skippable_internal_keys (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_read_tier)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_read_tier (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_int32(env, options->read_tier, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_read_tier (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_iter_start_seqnum)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_iter_start_seqnum (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_bigint_uint64(env, options->iter_start_seqnum, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_iter_start_seqnum (finished)";
    return result;
}

NAPI_METHOD(rocksdb_read_options_get_readahead_size)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_readahead_size (started)";

    NAPI_ARGV(1);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    napi_value result;
    NAPI_STATUS_THROWS(napi_create_bigint_uint64(env, options->readahead_size, &result));
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_get_readahead_size (finished)";
    return result;
}

// -------------------------------------------------

NAPI_METHOD(rocksdb_read_options_set_verify_checksums)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_verify_checksums (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->verify_checksums = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_verify_checksums (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_fill_cache)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_fill_cache (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->fill_cache = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_fill_cache (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_tailing)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_tailing (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->tailing = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_tailing (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_managed)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_managed (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->managed = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_managed (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_total_order_seek)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_total_order_seek (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->total_order_seek = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_total_order_seek (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_prefix_same_as_start)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_prefix_same_as_start (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->prefix_same_as_start = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_prefix_same_as_start (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_pin_data)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_pin_data (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->pin_data = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_pin_data (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_background_purge_on_iterator_cleanup)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_background_purge_on_iterator_cleanup (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->background_purge_on_iterator_cleanup = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_background_purge_on_iterator_cleanup (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_ignore_range_deletions)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_ignore_range_deletions (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool value;
    NAPI_STATUS_THROWS(napi_get_value_bool(env, argv[1], &value));

    options->ignore_range_deletions = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_ignore_range_deletions (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_max_skippable_internal_keys)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_max_skippable_internal_keys (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool lossless;
    uint64_t value;
    NAPI_STATUS_THROWS(napi_get_value_bigint_uint64(env, argv[1], &value, &lossless));

    options->max_skippable_internal_keys = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_max_skippable_internal_keys (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_read_tier)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_read_tier (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    int32_t value;
    NAPI_STATUS_THROWS(napi_get_value_int32(env, argv[1], &value));

    options->read_tier = (rocksdb::ReadTier)value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_read_tier (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_iter_start_seqnum)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_iter_start_seqnum (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool lossless;
    uint64_t value;
    NAPI_STATUS_THROWS(napi_get_value_bigint_uint64(env, argv[1], &value, &lossless));

    options->iter_start_seqnum = value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_iter_start_seqnum (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(rocksdb_read_options_set_readahead_size)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_readahead_size (started)";

    NAPI_ARGV(2);

    rocksdb::ReadOptions *FROM_EXTERNAL_ARGUMENT(0, options);

    bool lossless;
    uint64_t value;
    NAPI_STATUS_THROWS(napi_get_value_bigint_uint64(env, argv[1], &value, &lossless));

    options->readahead_size = (size_t)value;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " rocksdb_read_options_set_readahead_size (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(logger_config)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " logger_config (started)";

    NAPI_ARGV(1);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " logger_config (reading string)";

    STRING_FROM_BUFFER_ARGUMENT(0, logger_config_path)

    // LOG_IF(logging_enabled, INFO) << LOCATION << " logger_config (read string" << logger_config_path << ")";

    // Load configuration from file
    el::Configurations conf(logger_config_path);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " logger_config (reconfiguring logger)";

    // Reconfigure single logger
    el::Loggers::reconfigureLogger("default", conf);

    // LOG_IF(logging_enabled, INFO) << LOCATION << " logger_config (reconfiguring all loggers)";

    // Actually reconfigure all loggers instead
    el::Loggers::reconfigureAllLoggers(conf);

    // Now all the loggers will use configuration from file
    // LOG_IF(logging_enabled, INFO) << LOCATION << " logger_config (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(logger_start)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " logger_start (started)";

    NAPI_ARGV(0);

    logging_enabled = true;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " logger_start (finished)";
    NAPI_RETURN_UNDEFINED();
}

NAPI_METHOD(logger_stop)
{
    // LOG_IF(logging_enabled, INFO) << LOCATION << " logger_stop (started)";

    NAPI_ARGV(0);

    logging_enabled = false;

    // LOG_IF(logging_enabled, INFO) << LOCATION << " logger_stop (finished)";
    NAPI_RETURN_UNDEFINED();
}

/**
 * All exported functions.
 */
NAPI_INIT()
{
    NAPI_EXPORT_FUNCTION(rocksdb_slice);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_init);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_get_sync);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_get_disableWAL);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_get_ignore_missing_column_families);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_get_no_slowdown);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_get_low_pri);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_set_timestamp);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_reset_timestamp);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_set_sync);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_set_disableWAL);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_set_ignore_missing_column_families);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_set_no_slowdown);
    NAPI_EXPORT_FUNCTION(rocksdb_write_options_set_low_pri);
    NAPI_EXPORT_FUNCTION(rocksdb_options_init);
    NAPI_EXPORT_FUNCTION(rocksdb_options_init_from_buffer);
    NAPI_EXPORT_FUNCTION(rocksdb_open);
    NAPI_EXPORT_FUNCTION(rocksdb_close);
    NAPI_EXPORT_FUNCTION(rocksdb_put);
    NAPI_EXPORT_FUNCTION(rocksdb_put_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_put_latin1);
    NAPI_EXPORT_FUNCTION(rocksdb_get);
    NAPI_EXPORT_FUNCTION(rocksdb_get_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_get_latin1);
    NAPI_EXPORT_FUNCTION(rocksdb_delete);
    NAPI_EXPORT_FUNCTION(rocksdb_delete_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_delete_latin1);
    NAPI_EXPORT_FUNCTION(rocksdb_delete_range);
    NAPI_EXPORT_FUNCTION(rocksdb_delete_range_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_delete_range_latin1);
    NAPI_EXPORT_FUNCTION(rocksdb_destroy);
    NAPI_EXPORT_FUNCTION(rocksdb_repair);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_init);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_seek);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_seek_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_seek_latin1);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_seek_for_prev);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_seek_for_prev_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_seek_for_prev_latin1);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_seek_for_first);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_seek_for_last);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_reset);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_valid);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_refresh);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_next);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_prev);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_key);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_key_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_key_latin1);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_value);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_value_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_value_latin1);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_key_value_pair);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_key_value_pair_of_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_status_code);
    NAPI_EXPORT_FUNCTION(rocksdb_iterator_status_subcode);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_init);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_init_ex);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_put);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_put_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_put_latin1);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_delete);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_delete_utf8);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_delete_latin1);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_clear);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_write);
    NAPI_EXPORT_FUNCTION(rocksdb_batch_write_async);
    NAPI_EXPORT_FUNCTION(rocksdb_snapshot_init);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_init);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_snapshot);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_reset_snapshot);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_timestamp);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_reset_timestamp);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_iterate_lower_bound);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_reset_iterate_lower_bound);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_iterate_upper_bound);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_reset_iterate_upper_bound);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_verify_checksums);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_fill_cache);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_tailing);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_managed);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_total_order_seek);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_prefix_same_as_start);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_pin_data);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_background_purge_on_iterator_cleanup);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_ignore_range_deletions);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_max_skippable_internal_keys);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_read_tier);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_iter_start_seqnum);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_get_readahead_size);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_verify_checksums);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_fill_cache);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_tailing);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_managed);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_total_order_seek);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_prefix_same_as_start);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_pin_data);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_background_purge_on_iterator_cleanup);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_ignore_range_deletions);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_max_skippable_internal_keys);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_read_tier);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_iter_start_seqnum);
    NAPI_EXPORT_FUNCTION(rocksdb_read_options_set_readahead_size);
    NAPI_EXPORT_FUNCTION(logger_config);
    NAPI_EXPORT_FUNCTION(logger_start);
    NAPI_EXPORT_FUNCTION(logger_stop);
}
