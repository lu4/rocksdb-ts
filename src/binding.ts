export type Callback = (error: Error) => void;
export type CallbackWithValue<V> = (error: Error, value: V) => void;
export type CallbackWithKeyValue<K, V> = (error: Error, key: K, value: V) => void;

export type Unique<T> = T & { readonly '': unique symbol };

export enum BatchContext { }
export enum SliceContext { }
export enum SnapshotContext { }
export enum IteratorContext { }
export enum DatabaseContext { }
export enum OptionsContext { }
export enum ReadOptionsContext { }
export enum WriteOptionsContext { }

export enum IteratorStatusCode {
    Ok = 0,
    NotFound = 1,
    Corruption = 2,
    NotSupported = 3,
    InvalidArgument = 4,
    IOError = 5,
    MergeInProgress = 6,
    Incomplete = 7,
    ShutdownInProgress = 8,
    TimedOut = 9,
    Aborted = 10,
    Busy = 11,
    Expired = 12,
    TryAgain = 13,
    CompactionTooLarge = 14
}

export enum IteratorStatusSubcode {
    None = 0,
    MutexTimeout = 1,
    LockTimeout = 2,
    LockLimit = 3,
    NoSpace = 4,
    Deadlock = 5,
    StaleFile = 6,
    MemoryLimit = 7,
    MaxSubCode
}

export enum ReadOptionsReadTier {
    ReadAllTier = 0x0,
    BlockCacheTier = 0x1,
    PersistedTier = 0x2,
    MemtableTier = 0x3
}

export interface RocksBinding {
    rocksdb_slice(buffer: Buffer): Unique<SliceContext>;
    rocksdb_write_options_init(): Unique<WriteOptionsContext>;

    rocksdb_write_options_get_sync(context: Unique<WriteOptionsContext>): boolean;
    rocksdb_write_options_get_disableWAL(context: Unique<WriteOptionsContext>): boolean;
    rocksdb_write_options_get_ignore_missing_column_families(context: Unique<WriteOptionsContext>): boolean;
    rocksdb_write_options_get_no_slowdown(context: Unique<WriteOptionsContext>): boolean;
    rocksdb_write_options_get_low_pri(context: Unique<WriteOptionsContext>): boolean;

    rocksdb_write_options_set_timestamp(options: Unique<WriteOptionsContext>, context: Unique<SnapshotContext>): void;
    rocksdb_write_options_reset_timestamp(options: Unique<WriteOptionsContext>): void;
    rocksdb_write_options_set_sync(context: Unique<WriteOptionsContext>, value: boolean): void;
    rocksdb_write_options_set_disableWAL(context: Unique<WriteOptionsContext>, value: boolean): void;
    rocksdb_write_options_set_ignore_missing_column_families(context: Unique<WriteOptionsContext>, value: boolean): void;
    rocksdb_write_options_set_no_slowdown(context: Unique<WriteOptionsContext>, value: boolean): void;
    rocksdb_write_options_set_low_pri(context: Unique<WriteOptionsContext>, value: boolean): void;

    rocksdb_options_init(): Unique<OptionsContext>;
    rocksdb_options_init_from_buffer(options: Buffer): Unique<OptionsContext>;

    rocksdb_open(location: Buffer, options: Unique<OptionsContext>): Unique<DatabaseContext>;
    rocksdb_close(database: Unique<DatabaseContext>): void;
    rocksdb_put(database: Unique<DatabaseContext>, key: Buffer, value: Buffer, options: Unique<WriteOptionsContext>): void;
    rocksdb_put_utf8(database: Unique<DatabaseContext>, key: string, value: string, options: Unique<WriteOptionsContext>): void;
    rocksdb_put_latin1(database: Unique<DatabaseContext>, key: string, value: string, options: Unique<WriteOptionsContext>): void;
    rocksdb_get(database: Unique<DatabaseContext>, key: Buffer, options: Unique<ReadOptionsContext>): Buffer;
    rocksdb_get_utf8(database: Unique<DatabaseContext>, key: string, options: Unique<ReadOptionsContext>): string;
    rocksdb_get_latin1(database: Unique<DatabaseContext>, key: string, options: Unique<ReadOptionsContext>): string;
    rocksdb_delete(database: Unique<DatabaseContext>, key: Buffer, options: Unique<WriteOptionsContext>): void;
    rocksdb_delete_utf8(database: Unique<DatabaseContext>, key: string, options: Unique<WriteOptionsContext>): void;
    rocksdb_delete_latin1(database: Unique<DatabaseContext>, key: string, options: Unique<WriteOptionsContext>): void;
    rocksdb_delete_range(database: Unique<DatabaseContext>, from: Buffer, till: Buffer, options: Unique<WriteOptionsContext>): void;
    rocksdb_delete_range_utf8(database: Unique<DatabaseContext>, from: string, till: string, options: Unique<WriteOptionsContext>): void;
    rocksdb_delete_range_latin1(database: Unique<DatabaseContext>, from: string, till: string, options: Unique<WriteOptionsContext>): void;
    rocksdb_destroy(location: Buffer, options: Unique<OptionsContext>): void;
    rocksdb_repair(location: Buffer, options: Unique<OptionsContext>): void;

    rocksdb_iterator_init(database: Unique<DatabaseContext>, options: Unique<ReadOptionsContext>): Unique<IteratorContext>;
    rocksdb_iterator_seek(iterator: Unique<IteratorContext>, key: Buffer): boolean;
    rocksdb_iterator_seek_for_prev(iterator: Unique<IteratorContext>, key: Buffer): boolean;
    rocksdb_iterator_seek_utf8(iterator: Unique<IteratorContext>, key: string): boolean;
    rocksdb_iterator_seek_latin1(iterator: Unique<IteratorContext>, key: string): boolean;
    rocksdb_iterator_seek_for_prev_utf8(iterator: Unique<IteratorContext>, key: string): boolean;
    rocksdb_iterator_seek_for_prev_latin1(iterator: Unique<IteratorContext>, key: string): boolean;
    rocksdb_iterator_seek_for_first(iterator: Unique<IteratorContext>): boolean;
    rocksdb_iterator_seek_for_last(iterator: Unique<IteratorContext>): boolean;
    rocksdb_iterator_reset(iterator: Unique<IteratorContext>): boolean;
    rocksdb_iterator_valid(iterator: Unique<IteratorContext>): boolean;
    rocksdb_iterator_refresh(iterator: Unique<IteratorContext>): boolean;
    rocksdb_iterator_next(iterator: Unique<IteratorContext>): boolean;
    rocksdb_iterator_prev(iterator: Unique<IteratorContext>): boolean;
    rocksdb_iterator_key(iterator: Unique<IteratorContext>): Buffer;
    rocksdb_iterator_key_utf8(iterator: Unique<IteratorContext>): string;
    rocksdb_iterator_key_latin1(iterator: Unique<IteratorContext>): string;
    rocksdb_iterator_value(iterator: Unique<IteratorContext>): Buffer;
    rocksdb_iterator_value_utf8(iterator: Unique<IteratorContext>): string;
    rocksdb_iterator_value_latin1(iterator: Unique<IteratorContext>): string;
    rocksdb_iterator_key_value_pair(iterator: Unique<IteratorContext>): [Buffer, Buffer];
    rocksdb_iterator_key_value_pair_of_utf8(iterator: Unique<IteratorContext>): [string, string];
    rocksdb_iterator_status_code(iterator: Unique<IteratorContext>): IteratorStatusCode;
    rocksdb_iterator_status_subcode(iterator: Unique<IteratorContext>): IteratorStatusSubcode;

    rocksdb_batch_init(): Unique<BatchContext>;
    rocksdb_batch_init_ex(reservedBytes: bigint, maxBytes: bigint): Unique<BatchContext>;
    rocksdb_batch_put(batch: Unique<BatchContext>, key: Buffer, value: Buffer): void;
    rocksdb_batch_put_utf8(batch: Unique<BatchContext>, key: string, value: string): void;
    rocksdb_batch_put_latin1(batch: Unique<BatchContext>, key: string, value: string): void;
    rocksdb_batch_delete(batch: Unique<BatchContext>, key: Buffer): void;
    rocksdb_batch_delete_utf8(batch: Unique<BatchContext>, key: string): void;
    rocksdb_batch_delete_latin1(batch: Unique<BatchContext>, key: string): void;

    rocksdb_batch_clear(batch: Unique<BatchContext>): void;
    rocksdb_batch_write(database: Unique<DatabaseContext>, batch: Unique<BatchContext>, options: Unique<WriteOptionsContext>): void;
    rocksdb_batch_write_async(database: Unique<DatabaseContext>, batch: Unique<BatchContext>, options: Unique<WriteOptionsContext>): Promise<void>;

    rocksdb_snapshot_init(database: Unique<DatabaseContext>): Unique<SnapshotContext>;

    rocksdb_read_options_init(): Unique<ReadOptionsContext>;

    rocksdb_read_options_set_timestamp(options: Unique<ReadOptionsContext>, context: Unique<SliceContext>): void;
    rocksdb_read_options_reset_timestamp(options: Unique<ReadOptionsContext>): void;

    rocksdb_read_options_set_snapshot(options: Unique<ReadOptionsContext>, context: Unique<SnapshotContext>): void;
    rocksdb_read_options_reset_snapshot(options: Unique<ReadOptionsContext>): void;

    rocksdb_read_options_set_iterate_lower_bound(options: Unique<ReadOptionsContext>, value: Unique<SliceContext>): void;
    rocksdb_read_options_reset_iterate_lower_bound(options: Unique<ReadOptionsContext>): void;

    rocksdb_read_options_set_iterate_upper_bound(options: Unique<ReadOptionsContext>, value: Unique<SliceContext>): void;
    rocksdb_read_options_reset_iterate_upper_bound(options: Unique<ReadOptionsContext>): void;

    rocksdb_read_options_get_verify_checksums(options: Unique<ReadOptionsContext>): boolean;
    rocksdb_read_options_get_fill_cache(options: Unique<ReadOptionsContext>): boolean;
    rocksdb_read_options_get_tailing(options: Unique<ReadOptionsContext>): boolean;
    rocksdb_read_options_get_managed(options: Unique<ReadOptionsContext>): boolean;
    rocksdb_read_options_get_total_order_seek(options: Unique<ReadOptionsContext>): boolean;
    rocksdb_read_options_get_prefix_same_as_start(options: Unique<ReadOptionsContext>): boolean;
    rocksdb_read_options_get_pin_data(options: Unique<ReadOptionsContext>): boolean;
    rocksdb_read_options_get_background_purge_on_iterator_cleanup(options: Unique<ReadOptionsContext>): boolean;
    rocksdb_read_options_get_ignore_range_deletions(options: Unique<ReadOptionsContext>): boolean;
    rocksdb_read_options_get_max_skippable_internal_keys(options: Unique<ReadOptionsContext>): bigint;
    rocksdb_read_options_get_read_tier(options: Unique<ReadOptionsContext>): ReadOptionsReadTier;
    rocksdb_read_options_get_iter_start_seqnum(options: Unique<ReadOptionsContext>): bigint;
    rocksdb_read_options_get_readahead_size(options: Unique<ReadOptionsContext>): bigint;

    rocksdb_read_options_set_verify_checksums(options: Unique<ReadOptionsContext>, value: boolean): void;
    rocksdb_read_options_set_fill_cache(options: Unique<ReadOptionsContext>, value: boolean): void;
    rocksdb_read_options_set_tailing(options: Unique<ReadOptionsContext>, value: boolean): void;
    rocksdb_read_options_set_managed(options: Unique<ReadOptionsContext>, value: boolean): void;
    rocksdb_read_options_set_total_order_seek(options: Unique<ReadOptionsContext>, value: boolean): void;
    rocksdb_read_options_set_prefix_same_as_start(options: Unique<ReadOptionsContext>, value: boolean): void;
    rocksdb_read_options_set_pin_data(options: Unique<ReadOptionsContext>, value: boolean): void;
    rocksdb_read_options_set_background_purge_on_iterator_cleanup(options: Unique<ReadOptionsContext>, value: boolean): void;
    rocksdb_read_options_set_ignore_range_deletions(options: Unique<ReadOptionsContext>, value: boolean): void;
    rocksdb_read_options_set_max_skippable_internal_keys(options: Unique<ReadOptionsContext>, value: bigint): void;
    rocksdb_read_options_set_read_tier(options: Unique<ReadOptionsContext>, value: ReadOptionsReadTier): void;
    rocksdb_read_options_set_iter_start_seqnum(options: Unique<ReadOptionsContext>, value: bigint): void;
    rocksdb_read_options_set_readahead_size(options: Unique<ReadOptionsContext>, value: bigint): void;

    logger_config(configPath: Buffer): void;
    logger_start(): void;
    logger_stop(): void;
}
