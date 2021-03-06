const rocksdb_c_api_data = require("./c.h.json");

const functions = rocksdb_c_api_data
    .children
    .filter((d) => d.kind === "CursorKind.FUNCTION_DECL");


console.log("functions");
functions.forEach((v) => {
    console.log(v.spelling);
});

console.log("types");
let types = new Set([]);
functions.forEach((v) => {
    types = new Set([
        ...types,
        ...v.f_args.map(({ type }) => type)
    ]);
});

types.forEach((x) => {
    console.log(x);
})

console.log("ctypes");
let ctypes = new Set([]);
functions.forEach((v) => {
    ctypes = new Set([
        ...ctypes,
        ...v.f_args.map(({ spelling }) => spelling)
    ]);
});

ctypes.forEach((x) => {
    console.log(x);
})
