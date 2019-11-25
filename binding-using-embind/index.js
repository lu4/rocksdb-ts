'use strict';
const m = require("./a.out.js");


m.onRuntimeInitialized = _ => {
    console.log(m.Rocksdb);

    const options = m.cwrap('rocksdb_options_create', 'number', []);

    var x = new m.Rocksdb();
    // var o = x.getOptions();
    // var mm = m.rocksdb_options_create();
    var mm = options();
    console.log(mm);
   // console.log(x, o);
    // console.log(m.exclaim("hello world"));
};

