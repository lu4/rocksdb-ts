import { Unique, ReadOptionsContext, lib } from "./binding";
import { rockdbIterator, getEntry } from "./common";

export class RocksDbArray<T> implements ArrayLike<T> {
    public rocksdbOpts: any = null;
    public db: any = null;
    public arr = [];

    constructor(public location: string, public options: string) {
        this.rocksdbOpts = lib.rocksdb_options_init_from_buffer(Buffer.from(options));
        this.db = lib.rocksdb_open(Buffer.from(location, "utf-8"), this.rocksdbOpts);

        return new Proxy(this, {
            get(target: RocksDbArray<T>, key: any, receiver: any): T {
                const arrIndex = typeof (key) === "number" && Number.isInteger(key) && key >= 0;
                if (arrIndex) {
                    return target.arr[key];
                }
                else {
                    return target[key];
                }
            },
            set(target: RocksDbArray<T>, key: PropertyKey, value: any, receiver: any): boolean {
                const arrIndex = typeof (key) === "number" && Number.isInteger(key) && key >= 0;

                return true;
            }
        });
    }
    public get length(): number {
        return 0;
    }
    readonly [n: number]: T;
}
