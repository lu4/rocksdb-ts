

// class M extends Array {
//     constructor(...args) {
//         super(...args)

//         return new Proxy(this, {
//             get: function (target, name) {

//                 console.log("111: ", target, name)
//             }
//         });
//     }

//     get length () {
//         console.log(length);
//         return Infinity;
//     }
// }


class M extends Array {

    * [Symbol.iterator]() {
        yield 777;
        yield 999;
    }
}

var x = new M(1,2,3,"t");

for (key of x) {
    console.log(key, x[key]);
}









// const proxy = new Proxy(array, {
//     get(target, property) {
//       console.log(`Proxy get trap, property ${property}`);
//       return Reflect.get(target, property);
//     },
//   });
