// import { bindings, OptionsContext, Unique, ReadOptionsContext, DatabaseContext, IteratorContext } from "rocksdb";
// tslint:disable: max-classes-per-file
import path from 'path';
import { RocksDbMap } from "./RocksDbMap";

const dbOptionsStr = [
    "create_if_missing=true"
].join(";");

const dbPath = path.join(process.env.HOME!, "source-index-test.db");
const v = new RocksDbMap(dbPath, dbOptionsStr);

for (const x of v.entries()) {
    console.log((x as any)[0].toString(), (x as any)[1].toString());
}

console.log("done");
