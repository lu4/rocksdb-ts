declare module 'node-gyp-build' {
    import { RocksBinding } from "src/binding";
    export default function(path: string): RocksBinding;
}
