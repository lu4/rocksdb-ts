var nbind = require('nbind');
var lib = nbind.init().lib;
var Rocksdb = lib.Rocksdb;
var Options = lib.Options;

var opt = new Options();

var db = {};

var tdb = new Rocksdb();
console.log(tdb);
// var res = new TESTDB(db);
var status = tdb.Open(opt, "./db.db");

console.log("status: ", status.ok());
