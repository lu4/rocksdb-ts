import { Unique, ReadOptionsContext, DatabaseContext, IteratorContext, lib } from "./binding";

export const getEntry = (i: Unique<IteratorContext>): any => [lib.rocksdb_iterator_key(i), lib.rocksdb_iterator_value(i)];
const getKey = (i: Unique<IteratorContext>) => lib.rocksdb_iterator_key(i);
const getValue = (i: Unique<IteratorContext>) => lib.rocksdb_iterator_value(i);
export function* rockdbIterator(db: Unique<DatabaseContext>, rOpts: Unique<ReadOptionsContext>, startKey: any, getDataFn = getEntry) {
    const i = lib.rocksdb_iterator_init(db, rOpts);
    let hasValues = startKey ? lib.rocksdb_iterator_seek(i, startKey) : lib.rocksdb_iterator_seek_for_first(i);
    while (hasValues) {
        const nextKey = yield getDataFn(i);
        hasValues = nextKey ? lib.rocksdb_iterator_seek(i, nextKey) : lib.rocksdb_iterator_next(i);
    }
}
