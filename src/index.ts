import { NativeModules } from 'react-native';

let nativeModuleInitError: null | string = null;
const LeveldbModule = NativeModules.Leveldb;
if (LeveldbModule == null || typeof LeveldbModule.install !== 'function') {
  nativeModuleInitError =
    'The native Leveldb module could not be found! Is it correctly installed and autolinked?';
}

// Call the synchronous blocking install() function
try {
  if (LeveldbModule.install() !== true) {
    nativeModuleInitError =
      'The native Leveldb JSI bindings could not be installed!';
  }
} catch (e) {
  nativeModuleInitError =
    e instanceof Error ? e?.message : 'Leveldb.install caught error!';
}

const g = global as any;

export interface LevelDBIteratorI {
  // Position at the first key in the source.  The iterator is Valid()
  // after this call iff the source is not empty.
  seekToFirst(): LevelDBIteratorI;

  // Position at the last key in the source.  The iterator is
  // Valid() after this call iff the source is not empty.
  seekLast(): LevelDBIteratorI;

  // Position at the first key in the source that is at or past target.
  // The iterator is Valid() after this call iff the source contains
  // an entry that comes at or past target.
  seek(target: ArrayBuffer | string): LevelDBIteratorI;

  // An iterator is either positioned at a key/value pair, or
  // not valid.  This method returns true iff the iterator is valid.
  valid(): boolean;

  // Moves to the next entry in the source.  After this call, Valid() is
  // true iff the iterator was not positioned at the last entry in the source.
  // REQUIRES: Valid()
  next(): void;

  // Moves to the previous entry in the source.  After this call, Valid() is
  // true iff the iterator was not positioned at the first entry in source.
  // REQUIRES: Valid()
  prev(): void;
  close(): void;

  // Return the key for the current entry.  The underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // REQUIRES: Valid()
  keyStr(): string;
  keyBuf(): ArrayBuffer;

  // Return the value for the current entry.  The underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // REQUIRES: Valid()
  valueStr(): string;
  valueBuf(): ArrayBuffer;
}

export interface LevelDBI {
  // Close this ref to LevelDB.
  close(): void;

  // Returns true if this ref to LevelDB is closed. This can happen if close() is called on *any* open reference to a
  // given LevelDB name.
  closed(): boolean;

  // Set the database entry for "k" to "v".  Returns OK on success, throws an exception on error.
  put(k: ArrayBuffer | string, v: any): void;

  // Remove the database entry (if any) for "key". Throws an exception on error.
  // It is not an error if "key" did not exist in the database.
  delete(k: ArrayBuffer | string): void;

  // Returns the corresponding value for "key", if the database contains it; returns null otherwise.
  // Throws an exception if there is an error.
  // This returns a javascript object.
  get(k: ArrayBuffer | string): null | any;

  // Returns all the keys and values from the DB as a JS object
  getAllObjects(): Record<string, any>;

   // @deprecated: use getObject 
  getStr(k: ArrayBuffer | string): null | string;
  
  // @deprecated: use getAllObjects
  getAllStr(): Record<string, string>;



  // Returns an iterator over the contents of the database.
  // The result of newIterator() is initially invalid (caller must
  // call one of the seek methods on the iterator before using it).
  //
  // Caller should delete the iterator when it is no longer needed.
  // The returned iterator should be closed before this db is closed.
  newIterator(): LevelDBIteratorI;
}

export class LevelDBIterator implements LevelDBIteratorI {
  private ref: number;

  constructor(dbRef: number) {
    this.ref = g.leveldbNewIterator(dbRef);
  }

  seekToFirst(): LevelDBIterator {
    g.leveldbIteratorSeekToFirst(this.ref);
    return this;
  }

  seekLast(): LevelDBIterator {
    g.leveldbIteratorSeekToLast(this.ref);
    return this;
  }

  seek(target: ArrayBuffer | string): LevelDBIterator {
    g.leveldbIteratorSeek(this.ref, target);
    return this;
  }

  valid(): boolean {
    return g.leveldbIteratorValid(this.ref);
  }

  next(): void {
    g.leveldbIteratorNext(this.ref);
  }

  prev(): void {
    g.leveldbIteratorPrev(this.ref);
  }

  close() {
    g.leveldbIteratorDelete(this.ref);
    this.ref = -1;
  }

  keyStr(): string {
    return g.leveldbIteratorKeyStr(this.ref);
  }

  keyBuf(): ArrayBuffer {
    return g.leveldbIteratorKeyBuf(this.ref);
  }

  valueStr(): string {
    return g.leveldbIteratorValueStr(this.ref);
  }

  valueBuf(): ArrayBuffer {
    return g.leveldbIteratorValueBuf(this.ref);
  }
}

export class LevelDB implements LevelDBI {
  // Keep references to already open DBs here to facilitate RN's edit-refresh flow.
  // Note that when editing this file, this won't work, as RN will reload it and the openPathRefs
  // will be lost.
  private static openPathRefs: { [name: string]: undefined | number } = {};
  private ref: undefined | number;

  constructor(name: string, createIfMissing: boolean, errorIfExists: boolean) {
    if (nativeModuleInitError) {
      throw new Error(nativeModuleInitError);
    }

    if (LevelDB.openPathRefs[name] !== undefined) {
      this.ref = LevelDB.openPathRefs[name];
    } else {
      LevelDB.openPathRefs[name] = this.ref = g.leveldbOpen(
        name,
        createIfMissing,
        errorIfExists
      );
    }
  }

  close() {
    g.leveldbClose(this.ref);
    for (const name in LevelDB.openPathRefs) {
      if (LevelDB.openPathRefs[name] === this.ref) {
        delete LevelDB.openPathRefs[name];
      }
    }
    this.ref = undefined;
  }

  closed(): boolean {
    return (
      this.ref === undefined ||
      !Object.values(LevelDB.openPathRefs).includes(this.ref)
    );
  }

  put(k: ArrayBuffer | string, v: any) {
    g.leveldbPut(this.ref, k, v);
  }

  get(k: string | ArrayBuffer) {
    return g.leveldbGet(this.ref, k);
  }

  clear() {
    g.leveldbClear(this.ref);
  }

  delete(k: ArrayBuffer | string) {
    g.leveldbDelete(this.ref, k);
  }
  
  getStr(k: ArrayBuffer | string): null | string {
    return g.leveldbGetStr(this.ref, k);
  }
  
  getAllStr(): Record<string, string> {
    return g.leveldbGetAllStr(this.ref);
  }

  getAllObjects(): Record<string, any> {
    return g.leveldbGetAllObjects(this.ref);
  }

  batchObjects(record: Record<string, any>, keysToDelete: string[] = []) {
    return g.leveldbBatchObjects(this.ref, record, keysToDelete);
  }

  newIterator(): LevelDBIterator {
    if (this.ref === undefined) {
      throw new Error(
        'LevelDB.newIterator: could not create iterator, the DB was closed!'
      );
    }
    return new LevelDBIterator(this.ref);
  }

  // Merges the data from another LevelDB into this one. All keys from src will be written into this LevelDB,
  // overwriting any existing values.
  // batchMerge=true will write all values from src in one transaction, thus ensuring that the dst DB is not left
  // in a corrupt state.
  merge(src: LevelDB, batchMerge: boolean) {
    if (this.ref === undefined) {
      throw new Error(
        'LevelDB.merge: could not merge, the dest DB (this) was closed!'
      );
    }
    if (src.ref === undefined) {
      throw new Error(
        'LevelDB.merge: could not merge, the source DB was closed!'
      );
    }
    g.leveldbMerge(this.ref, src.ref, batchMerge);
  }

  static destroyDB(name: string, force?: boolean) {
    if (LevelDB.openPathRefs[name] !== undefined) {
      if (force) {
        g.leveldbClose(LevelDB.openPathRefs[name]);
        delete LevelDB.openPathRefs[name];
      } else {
        throw new Error('DB is open! Cannot destroy');
      }
    }

    g.leveldbDestroy(name);
  }

  static readFileToBuf = g.leveldbReadFileBuf as (
    path: string,
    pos: number,
    len: number
  ) => ArrayBuffer;
}
