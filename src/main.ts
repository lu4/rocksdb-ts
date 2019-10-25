export * from './binding';

import binding from './binding';

// import {
//     DbContext,
//     BatchContext,
//     DelOptions,
//     GetOptions,
//     OpenOptions,
//     StringOrBuffer,
//     IteratorOptions,
//     BatchedOperation,
//     IteratorContext,
// } from './binding';

// function promisify<T>(resolve: (value?: T) => void, reject: (error: any) => void): (error?: Error, value?: T) => void {
//     return (error?: Error, value?: T) => {
//         if (error) {
//             reject(error);
//         } else {
//             resolve(value);
//         }
//     };
// }

export const bindings = binding;

// export class RocksDB {
//     private readonly ctx: DbContext;

//     public constructor(public readonly location: string) {
//         this.ctx = binding.db_init();
//     }

//     public open(options?: OpenOptions) {
//         return new Promise<void>((resolve, reject) => binding.db_open(this.ctx, this.location, options || {}, promisify(resolve, reject)));
//     }

//     public approximateSize(start: StringOrBuffer, end: StringOrBuffer) {
//         return new Promise<number>((resolve, reject) => binding.db_approximate_size(this.ctx, start, end, promisify(resolve, reject)));
//     }

//     public async batchOperations(batch: BatchedOperation[], options?: { sync?: boolean; }): Promise<void> {
//         return new Promise<void>(
//             (resolve, reject) => binding.batch_do(this.ctx, batch, options || {}, promisify(resolve, reject))
//         );
//     }

//     public async batch(): Promise<Batch> {
//         return new Batch(binding.batch_init(this.ctx));
//     }

//     public close() {
//         return new Promise<void>((resolve, reject) => binding.db_close(this.ctx, promisify(resolve, reject)));
//     }

//     public compactRange(start: StringOrBuffer, end: StringOrBuffer) {
//         return new Promise<void>((resolve, reject) => binding.db_compact_range(this.ctx, start, end, promisify(resolve, reject)));
//     }

//     public del(key: StringOrBuffer, options?: DelOptions) {
//         return new Promise<void>((resolve, reject) => binding.db_del(this.ctx, key, options || {}, promisify(resolve, reject)));
//     }

//     public destroy(location: string) {
//         return new Promise<void>((resolve, reject) => binding.destroy_db(location, promisify(resolve, reject)));
//     }

//     public get(key: StringOrBuffer, options?: GetOptions) {
//         return new Promise<Buffer>((resolve, reject) => binding.db_get(this.ctx, key, options || {}, promisify(resolve, reject) as any));
//     }

//     public getProperty(property: string) {
//         return binding.db_get_property(this.ctx, property);
//     }

//     public iterator(options?: IteratorOptions) {
//         if (options) {
//             return new Iterator(this, binding.iterator(options));
//         } else {
//             return new Iterator(this, this.ctx.iterator());
//         }
//     }
//     public put(key: StringOrBuffer, value: StringOrBuffer) {
//         return new Promise<void>((resolve, reject) => this.ctx.put(key, value, promisify(resolve, reject)));
//     }
//     public repair(location: string) {
//         return new Promise<void>((resolve, reject) => this.ctx.repair(location, promisify(resolve, reject)));
//     }
// }


// // tslint:disable-next-line: max-classes-per-file
// export class Iterator {
//     public constructor(private ctx: IteratorContext) {
//     }
// }

// // tslint:disable-next-line: max-classes-per-file
// export class Batch {
//     public constructor(private ctx: BatchContext) {

//     }

//     public put(key: StringOrBuffer, value: StringOrBuffer): this {
//         binding.batch_put(this.ctx, key, value);

//         return this;
//     }

//     public del(key: StringOrBuffer): this {
//         binding.batch_del(this.ctx, key);

//         return this;
//     }

//     public clear(): this {
//         binding.batch_clear(this.ctx);

//         return this;
//     }

//     public write(options?: { sync?: boolean }): Promise<any> {
//         return new Promise<any>((resolve, reject) => binding.batch_write(this.ctx, options || {}, promisify(resolve, reject)));
//     }
// }
