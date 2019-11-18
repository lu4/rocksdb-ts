var lib = require('bindings')('rocks_db');
const path = require('path');
const dbPath = path.join(process.env.HOME, "source-index-test1.db");

var Rocksdb = lib.Rocksdb;
var Options = lib.Options;

var opt = new Options();

var db = {};

var tdb = new Rocksdb();
console.log(tdb);
// var res = new TESTDB(db);
var status = tdb.Open(opt, dbPath);

console.log("status: ", status.ok());
