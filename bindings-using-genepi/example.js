
var lib = require('bindings')('rocks_db')

console.log(lib);

var Rocksdb = lib.Rocksdb;
var Options = lib.Options;

var opt = new Options();

var db = {};

var tdb = new Rocksdb();
console.log(tdb);

var status = tdb.Open("create_if_missing=true;", "~/db.db");

console.log("status: ", status.ok());
