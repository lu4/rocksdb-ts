import { Unique, ReadOptionsContext, lib } from "./binding";
import { rockdbIterator, getEntry } from "./common";

export class RocksDbMap<K, V> implements Map<K, V> {
    public rocksdbOpts: any = null;
    public db: any = null;
    constructor(public location: string, public options: string) {
        this.rocksdbOpts = lib.rocksdb_options_init_from_buffer(Buffer.from(options));
        this.db = lib.rocksdb_open(Buffer.from(location, "utf-8"), this.rocksdbOpts);
    }
    public [Symbol.toStringTag] = "RocksDbMap";
    public [Symbol.iterator](): IterableIterator<[K, V]> {
        return [][Symbol.iterator]();
    }
    public entries(rOpts?: Unique<ReadOptionsContext>): IterableIterator<[K, V]>;
    public entries(rOpts?: Unique<ReadOptionsContext>, startKey?: any): IterableIterator<[K, V]> {
        return rockdbIterator(this.db, rOpts || lib.rocksdb_read_options_init(), startKey, getEntry);
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
