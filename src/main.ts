// import { bindings, OptionsContext, Unique, ReadOptionsContext, DatabaseContext, IteratorContext } from "rocksdb";

import { Unique, ReadOptionsContext, DatabaseContext, IteratorContext } from "./binding";

import path from 'path';
import builder from 'node-gyp-build';
export const bindings = builder(path.resolve(path.join(__dirname, '..')));


const dbOptionsStr = [
    "create_if_missing=true"
].join(";")

const getEntry = (i: Unique<IteratorContext>): any => [bindings.rocksdb_iterator_key(i), bindings.rocksdb_iterator_value(i)];
const getKey = (i: Unique<IteratorContext>) => bindings.rocksdb_iterator_key(i);
const getValue = (i: Unique<IteratorContext>) => bindings.rocksdb_iterator_value(i);

function* rockdbIteratorSample(db: Unique<DatabaseContext>, rOpts: Unique<ReadOptionsContext>, startKey: any, getDataFn = getEntry) {
    const i = bindings.rocksdb_iterator_init(db, rOpts);

    let hasValues = startKey ? bindings.rocksdb_iterator_seek(i, startKey) : bindings.rocksdb_iterator_seek_for_first(i);

    while (hasValues) {
        const nextKey = yield getDataFn(i);
        hasValues = nextKey ? bindings.rocksdb_iterator_seek(i, nextKey) : bindings.rocksdb_iterator_next(i);
    }
}

export class RocksDbMapSample<K, V> implements Map<K, V> {
    public rocksdbOpts: any = null;
    public db: any = null;

    constructor (public location: string, public options: string) {
        this.rocksdbOpts = bindings.rocksdb_options_init_from_buffer(Buffer.from(options));
        this.db = bindings.rocksdb_open(Buffer.from(location, "utf-8"), this.rocksdbOpts);
    }

    public [Symbol.toStringTag] = "RocksDbMap";

    public [Symbol.iterator](): IterableIterator<[K, V]> {
        return [][Symbol.iterator]();
    }

    public entries(rOpts?: Unique<ReadOptionsContext>): IterableIterator<[K, V]>
    public entries(rOpts?: Unique<ReadOptionsContext>, startKey?: any): IterableIterator<[K, V]> {
        return rockdbIteratorSample(this.db, rOpts || bindings.rocksdb_read_options_init(), startKey, getEntry);
    }

    public keys(): IterableIterator<K> {
        return [][Symbol.iterator]();
    }

    public values(): IterableIterator<V> {
        return [][Symbol.iterator]();
    }

    public clear(): void {
        return void 0;
    }

    public delete(key: K): boolean {
        return true;
    }

    public forEach(callbackfn: (value: V, key: K, map: Map<K, V>) => void, thisArg?: any): void {
        return void 0;
    }

    public get(key: K): V | undefined {
        return undefined;
    }

    public has(key: K): boolean {
        return true;
    }

    public set(key: K, value: V): this {
        return this;
    }

    public readonly size: number = 0;
}


const dbPath = path.join(process.env.HOME!, "source-index-test.db");
const v = new RocksDbMapSample(dbPath, dbOptionsStr);

for (const x of v.entries()) {
    console.log((x as any)[0].toString(), (x as any)[1].toString());
}

console.log("done");
