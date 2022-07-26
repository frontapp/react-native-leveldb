#import "react-native-leveldb.h"
#import "packer.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <leveldb/filter_policy.h>

using namespace facebook;

// TODO(savv): consider re-using unique_ptrs, if they are empty.
std::vector<std::unique_ptr<leveldb::DB>> dbs;
std::vector<std::unique_ptr<leveldb::Iterator>> iterators;

// Returns false if the passed value is not a string or an ArrayBuffer.
bool valueToString(jsi::Runtime& runtime, const jsi::Value& value, std::string* str) {
  if (value.isString()) {
    *str = value.asString(runtime).utf8(runtime);
    return true;
  }

  if (value.isObject()) {
    auto obj = value.asObject(runtime);
    if (!obj.isArrayBuffer(runtime)) {
      return false;
    }
    auto buf = obj.getArrayBuffer(runtime);
    *str = std::string((char*)buf.data(runtime), buf.size(runtime));
    return true;
  }

  return false;
}

leveldb::DB* valueToDb(const jsi::Value& value, std::string* err) {
  if (!value.isNumber()) {
    *err = "valueToDb/param-not-a-number";
    return nullptr;
  }
  int idx = (int)value.getNumber();
  if (idx < 0 || idx >= dbs.size()) {
    *err = "valueToDb/idx-out-of-range";
    return nullptr;
  }
  if (!dbs[idx].get()) {
    *err = "valueToDb/db-closed";
    return nullptr;
  }

  return dbs[idx].get();
}

leveldb::Iterator* valueToIterator(const jsi::Value& value) {
  if (!value.isNumber()) {
    return nullptr;
  }
  int idx = (int)value.getNumber();
  if (idx < 0 || idx >= iterators.size()) {
    return nullptr;
  }

  return iterators[idx].get();
}

void installLeveldb(jsi::Runtime& jsiRuntime, std::string documentDir) {
  if (documentDir[documentDir.length() - 1] != '/') {
    documentDir += '/';
  }
  std::cout << "Initializing react-native-leveldb with document dir \"" << documentDir << "\"" << "\n";

  auto leveldbOpen = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbOpen"),
      3,  // db path, create_if_missing, error_if_exists
      [documentDir](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        if (!arguments[0].isString() || !arguments[1].isBool() || !arguments[2].isBool()) {
          throw jsi::JSError(runtime, "leveldbOpen/invalid-params");
        }

        leveldb::Options options;
        std::string path = documentDir + arguments[0].getString(runtime).utf8(runtime);
        options.create_if_missing = arguments[1].getBool();
        options.error_if_exists = arguments[2].getBool();
        options.compression = leveldb::CompressionType::kNoCompression;
        options.filter_policy = leveldb::NewBloomFilterPolicy(10);
        options.reuse_logs = true;

        leveldb::DB* db;
        leveldb::Status status = leveldb::DB::Open(options, path, &db);

        dbs.push_back(std::unique_ptr<leveldb::DB>{db});
        int idx = (int)dbs.size() - 1;

        if (!status.ok()) {
          dbs[idx].reset();
          throw jsi::JSError(runtime, "leveldbOpen/" + status.ToString());
        }

        return jsi::Value(idx);
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbOpen", std::move(leveldbOpen));

  auto leveldbDestroy = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbDestroy"),
      1,  // db path
      [documentDir](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        if (!arguments[0].isString()) {
          throw jsi::JSError(runtime, "leveldbDestroy/invalid-params");
        }

        leveldb::Options options;
        std::string path = documentDir + arguments[0].getString(runtime).utf8(runtime);
        leveldb::Status status = leveldb::DestroyDB(path, options);
        if (!status.ok()) {
          throw jsi::JSError(runtime, "leveldbDestroy/" + status.ToString());
        }

        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbDestroy", std::move(leveldbDestroy));

  auto leveldbClose = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbClose"),
      1,  // dbs index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        if (!arguments[0].isNumber()) {
          throw jsi::JSError(runtime, "leveldbClose/invalid-params");
        }
        int idx = (int)arguments[0].getNumber();
        if (idx < 0 || idx >= dbs.size() || !dbs[idx].get()) {
          throw jsi::JSError(runtime, "leveldbClose/db-idx-out-of-bounds");
        }

        dbs[idx].reset();
        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbClose", std::move(leveldbClose));
      
  auto leveldbGetStr = jsi::Function::createFromHostFunction(
       jsiRuntime,
       jsi::PropNameID::forAscii(jsiRuntime, "leveldbGetStr"),
       2,  // dbs index, key
       [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
         std::string dbErr;
         leveldb::DB* db = valueToDb(arguments[0], &dbErr);
         if (!db) {
           throw jsi::JSError(runtime, "leveldbGetStr/" + dbErr);
         }
         std::string key;
         if (!valueToString(runtime, arguments[1], &key)) {
           throw jsi::JSError(runtime, "leveldbGetStr/invalid-params");
         }

         std::string value;
         auto status = db->Get(leveldb::ReadOptions(), key, &value);
         if (status.IsNotFound()) {
           return nullptr;
         } else if (!status.ok()) {
           throw jsi::JSError(runtime, "leveldbGetStr/" + status.ToString());
         }
         return jsi::Value(jsi::String::createFromUtf8(runtime, value));
       }
   );
   jsiRuntime.global().setProperty(jsiRuntime, "leveldbGetStr", std::move(leveldbGetStr));

   auto leveldbGetAllStr = jsi::Function::createFromHostFunction(
     jsiRuntime,
     jsi::PropNameID::forAscii(jsiRuntime, "leveldbGetAllStr"),
     1,  // dbs index
     [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
       std::string dbErr;
       leveldb::DB* db = valueToDb(arguments[0], &dbErr);
       if (!db) {
         throw jsi::JSError(runtime, "leveldbGetAllStr/" + dbErr);
       }
       auto result = jsi::Object(runtime);
       
       leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
       for (it->SeekToFirst(); it->Valid(); it->Next()) {
         auto key = jsi::String::createFromUtf8(runtime, it->key().ToString());
         auto value = jsi::String::createFromUtf8(runtime, it->value().ToString());
         result.setProperty(runtime, key, value);
       }
       assert(it->status().ok());  // Check for any errors found during the scan
       delete it;
       return result;
     }
   );
   jsiRuntime.global().setProperty(jsiRuntime, "leveldbGetAllStr", std::move(leveldbGetAllStr));
     

  auto leveldbPut = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbPut"),
      3,  // dbs index, key, value
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        std::string key;
        std::string dbErr;
        leveldb::DB* db = valueToDb(arguments[0], &dbErr);
        
        if (!db) {
          throw jsi::JSError(runtime, "leveldbPut/" + dbErr);
        }
          
        if (!valueToString(runtime, arguments[1], &key)) {
          throw jsi::JSError(runtime, "leveldbPut/invalid-params");
        }

        mpack_writer_t writer;
        size_t size;
        char* growable_buf;

        try {
            mpack_writer_init_growable(&writer, &growable_buf, &size);
            Packer::pack(arguments[2], runtime, &writer);

            if (mpack_writer_destroy(&writer) != mpack_ok) {
                throw jsi::JSError(runtime, "leveldbPut/ an error occured encoding the data");
            }

            auto status = db->Put(leveldb::WriteOptions(), key, leveldb::Slice(growable_buf, size));
            if (!status.ok()) {
                throw jsi::JSError(runtime, "leveldbPut/" + status.ToString());
            }
        } catch (...) {
            // safe to call twice
            mpack_writer_destroy(&writer);
            MPACK_FREE(growable_buf);
            throw;
        }
        MPACK_FREE(growable_buf);

        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbPut", std::move(leveldbPut));
    
  auto leveldbBatchObjects = jsi::Function::createFromHostFunction(
    jsiRuntime,
    jsi::PropNameID::forAscii(jsiRuntime, "leveldbBatchObjects"),
    3,  // dbs index, recordsToAdd, keysToDelete
    [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
      
 
      jsi::Object record = arguments[1].asObject(runtime);
      jsi::Array keysToDelete = arguments[2].asObject(runtime).asArray(runtime);
      
      std::string dbErr;
      leveldb::DB* db = valueToDb(arguments[0], &dbErr);

      if (!db) {
        throw jsi::JSError(runtime, "leveldbBatchObjects/" + dbErr);
      }

      leveldb::WriteBatch batch;
      auto names = record.getPropertyNames(runtime);
      auto length = names.length(runtime);
     
      mpack_writer_t writer;
      size_t size;
      char* growable_buf;
      try {
          for (size_t i = 0; i < length; i++) {

              auto key = names.getValueAtIndex(runtime, i).asString(runtime);
              mpack_writer_init_growable(&writer, &growable_buf, &size);
              Packer::pack(record.getProperty(runtime, key), runtime, &writer);

              if (mpack_writer_destroy(&writer) != mpack_ok) {
                  throw jsi::JSError(runtime, "leveldbBatchObjects/ an error occurred encoding the data");
              }

              batch.Put(key.utf8(runtime), leveldb::Slice(growable_buf, size));
              MPACK_FREE(growable_buf);
          }
      } catch(...) {
          // safe to call twice
          mpack_writer_destroy(&writer);
          MPACK_FREE(growable_buf);
          throw;
      }
      
      auto keysToDeleteLength = keysToDelete.length(runtime);
      for(size_t i = 0; i < keysToDeleteLength; i++) {
        batch.Delete(keysToDelete.getValueAtIndex(runtime, i).asString(runtime).utf8(runtime));
      }
      db->Write(leveldb::WriteOptions(), &batch);
      return nullptr;
    }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbBatchObjects", std::move(leveldbBatchObjects));
  
  auto leveldbClear = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbClear"),
      1,  // dbs index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        
        std::string dbErr;
        leveldb::DB* db = valueToDb(arguments[0], &dbErr);
        if (!db) {
          throw jsi::JSError(runtime, "leveldbClear/" + dbErr);
        }
        
        leveldb::WriteBatch batch;
        leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
          batch.Delete(it->key());
        }
        assert(it->status().ok());  // Check for any errors found during the scan
        delete it;
        
        db->Write(leveldb::WriteOptions(), &batch);
        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbClear", std::move(leveldbClear));


  auto leveldbDelete = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbDelete"),
      2,  // dbs index, key
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        std::string key;
        std::string dbErr;
        leveldb::DB* db = valueToDb(arguments[0], &dbErr);
        if (!db) {
          throw jsi::JSError(runtime, "leveldbDelete/" + dbErr);
        }
        if (!valueToString(runtime, arguments[1], &key)) {
          throw jsi::JSError(runtime, "leveldbDelete/invalid-params");
        }

        auto status = db->Delete(leveldb::WriteOptions(), key);

        if (status.ok() || status.IsNotFound()) {
          return nullptr;
        } else {
          throw jsi::JSError(runtime, "leveldbDelete/" + status.ToString());
        }
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbDelete", std::move(leveldbDelete));

  auto leveldbNewIterator = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbNewIterator"),
      1,  // index into dbs vector
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        std::string dbErr;
        leveldb::DB* db = valueToDb(arguments[0], &dbErr);
        if (!db) {
          throw jsi::JSError(runtime, "leveldbNewIterator/" + dbErr);
        }
        iterators.push_back(std::unique_ptr<leveldb::Iterator>{db->NewIterator(leveldb::ReadOptions())});
        return jsi::Value((int)iterators.size() - 1);
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbNewIterator", std::move(leveldbNewIterator));

  auto leveldbIteratorSeekToFirst = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorSeekToFirst"),
      1,  // iterators index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorSeekToFirst/invalid-params");
        }
        iterator->SeekToFirst();
        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorSeekToFirst", std::move(leveldbIteratorSeekToFirst));

  auto leveldbIteratorSeekToLast = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorSeekToLast"),
      1,  // iterators index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorSeekToLast/invalid-params");
        }
        iterator->SeekToLast();
        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorSeekToLast", std::move(leveldbIteratorSeekToLast));

  auto leveldbIteratorSeek = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorSeek"),
      2,  // iterators index, seek target
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorSeek/invalid-params");
        }

        std::string target;
        if (!valueToString(runtime, arguments[1], &target)) {
          throw jsi::JSError(runtime, "leveldbIteratorSeek/invalid-params");
        }
        iterator->Seek(target);
        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorSeek", std::move(leveldbIteratorSeek));

  auto leveldbIteratorValid = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorValid"),
      1,  // iterators index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorValid/invalid-params");
        }
        return jsi::Value(iterator->Valid());
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorValid", std::move(leveldbIteratorValid));

  auto leveldbIteratorPrev = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorPrev"),
      1,  // iterators index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorPrev/invalid-params");
        }
        iterator->Prev();
        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorPrev", std::move(leveldbIteratorPrev));

  auto leveldbIteratorNext = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorNext"),
      1,  // iterators index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorNext/invalid-params");
        }
        iterator->Next();
        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorNext", std::move(leveldbIteratorNext));

  auto leveldbIteratorDelete = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorDelete"),
      1,  // iterators index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorDelete/invalid-params");
        }
        iterators[(int)arguments[0].getNumber()].reset();
        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorDelete", std::move(leveldbIteratorDelete));

  auto leveldbIteratorKeyStr = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorKeyStr"),
      1,  // iterators index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorKeyStr/invalid-params");
        }
        return jsi::Value(jsi::String::createFromUtf8(runtime, iterator->key().ToString()));;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorKeyStr", std::move(leveldbIteratorKeyStr));

  auto leveldbIteratorValueStr = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorValueStr"),
      1,  // iterators index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorValueStr/invalid-params");
        }
        return jsi::Value(jsi::String::createFromUtf8(runtime, iterator->value().ToString()));;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorValueStr", std::move(leveldbIteratorValueStr));

  auto leveldbGet = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbGet"),
      2,  // dbs index, key
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        std::string dbErr;
        leveldb::DB* db = valueToDb(arguments[0], &dbErr);
        if (!db) {
          throw jsi::JSError(runtime, "leveldbGet/" + dbErr);
        }
        std::string key;
        if (!valueToString(runtime, arguments[1], &key)) {
          throw jsi::JSError(runtime, "leveldbGet/invalid-params");
        }
        std::string value;

        auto status = db->Get(leveldb::ReadOptions(), key, &value);

        if (status.IsNotFound()) {
          return nullptr;
        } else if (!status.ok()) {
          throw jsi::JSError(runtime, "leveldbGet/" + status.ToString());
        }
          
        mpack_reader_t reader;
        jsi::Value parsed;

        try {
            mpack_reader_init_data(&reader, value.data(), value.size());
            parsed = Packer::unpackElement(runtime, &reader, 0);
        } catch(...) {
            mpack_reader_destroy(&reader);
            throw;
        }

        if(mpack_ok != mpack_reader_destroy(&reader)) {
            throw jsi::JSError(runtime, "leveldbGet/ failed to read data");
        }
                    
        return parsed;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbGet", std::move(leveldbGet));

 auto leveldbGetAllObjects = jsi::Function::createFromHostFunction(
   jsiRuntime,
   jsi::PropNameID::forAscii(jsiRuntime, "leveldbGetAllObjects"),
   1,  // dbs index
   [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
     std::string dbErr;
     leveldb::DB* db = valueToDb(arguments[0], &dbErr);
     if (!db) {
       throw jsi::JSError(runtime, "leveldbGetAllObjects/" + dbErr);
     }
     auto result = jsi::Object(runtime);

     leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
     
     mpack_reader_t reader;

     try {
         for (it->SeekToFirst(); it->Valid(); it->Next()) {
             auto key = jsi::String::createFromUtf8(runtime, it->key().ToString());
             auto value = it->value();

             mpack_reader_init_data(&reader, value.data(), value.size());
             auto parsed = Packer::unpackElement(runtime, &reader, 0);

             if (mpack_ok != mpack_reader_destroy(&reader)) {
                 throw jsi::JSError(runtime, "leveldbGetAllObjects/ failed to read data");
             }

             result.setProperty(runtime, key, parsed);
         }
     }catch(...) {
         mpack_reader_destroy(&reader);
         throw;
     }
     assert(it->status().ok());  // Check for any errors found during the scan
     delete it;
     return result;
   }
 );
 jsiRuntime.global().setProperty(jsiRuntime, "leveldbGetAllObjects", std::move(leveldbGetAllObjects));
    
    
  auto leveldbIteratorKeyBuf = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorKeyBuf"),
      1,  // iterators index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorKeyBuf/invalid-params");
        }
        std::string key = iterator->key().ToString();

        jsi::Function arrayBufferCtor = runtime.global().getPropertyAsFunction(runtime, "ArrayBuffer");
        jsi::Object o = arrayBufferCtor.callAsConstructor(runtime, (int)key.length()).getObject(runtime);
        jsi::ArrayBuffer buf = o.getArrayBuffer(runtime);
        memcpy(buf.data(runtime), key.c_str(), key.size());
        return o;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorKeyBuf", std::move(leveldbIteratorKeyBuf));

  auto leveldbIteratorValueBuf = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbIteratorValueBuf"),
      1,  // iterators index
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        leveldb::Iterator* iterator = valueToIterator(arguments[0]);
        if (!iterator) {
          throw jsi::JSError(runtime, "leveldbIteratorValueBuf/invalid-params");
        }
        std::string value = iterator->value().ToString();

        jsi::Function arrayBufferCtor = runtime.global().getPropertyAsFunction(runtime, "ArrayBuffer");
        jsi::Object o = arrayBufferCtor.callAsConstructor(runtime, (int)value.length()).getObject(runtime);
        jsi::ArrayBuffer buf = o.getArrayBuffer(runtime);
        memcpy(buf.data(runtime), value.c_str(), value.size());
        return o;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbIteratorValueBuf", std::move(leveldbIteratorValueBuf));


  auto leveldbTestException = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbTestException"),
      0,
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        throw jsi::JSError(runtime, "leveldbTestException");
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbTestException", std::move(leveldbTestException));

  auto leveldbMerge = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbMerge"),
      3,  // dbs index dest, dbs index src, batchBool
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        std::string dbErr;
        leveldb::DB* dbDst = valueToDb(arguments[0], &dbErr);
        if (!dbDst) {
          throw jsi::JSError(runtime, "leveldbMerge/dst/" + dbErr);
        }
        leveldb::DB* dbSrc = valueToDb(arguments[1], &dbErr);
        if (!dbSrc) {
          throw jsi::JSError(runtime, "leveldbMerge/src/" + dbErr);
        }
        if (!arguments[2].isBool()) {
          throw jsi::JSError(runtime, "leveldbMerge/batchMerge-param-not-a-boolean");
        }
        bool batchMerge = (bool)arguments[2].getBool();

        leveldb::WriteBatch batch;
        std::unique_ptr<leveldb::Iterator> itSrc(dbSrc->NewIterator(leveldb::ReadOptions()));
        for (itSrc->SeekToFirst(); itSrc->Valid(); itSrc->Next()) {
          if (batchMerge) {
            batch.Put(itSrc->key(), itSrc->value());
          } else {
            dbDst->Put(leveldb::WriteOptions(), itSrc->key(), itSrc->value());
          }
        }

        if (!itSrc->status().ok()) {
          throw jsi::JSError(runtime, "leveldbMerge/" + itSrc->status().ToString());
        }

        if (batchMerge) {
          dbDst->Write(leveldb::WriteOptions(), &batch);
        }

        return nullptr;
      }
  );
  jsiRuntime.global().setProperty(jsiRuntime, "leveldbMerge", std::move(leveldbMerge));

  auto leveldbReadFileBuf = jsi::Function::createFromHostFunction(
      jsiRuntime,
      jsi::PropNameID::forAscii(jsiRuntime, "leveldbReadFileBuf"),
      3,  // path, pos, len
      [](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* arguments, size_t count) -> jsi::Value {
        std::string path;
        if (!valueToString(runtime, arguments[0], &path) || !arguments[1].isNumber() || !arguments[2].isNumber()) {
          throw jsi::JSError(runtime, "leveldbReadFileBuf/invalid-params");
        }
        int pos = (int)arguments[1].getNumber(), len = (int)arguments[2].getNumber();
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file || !file.is_open()) {
          throw jsi::JSError(runtime, "leveldbReadFileBuf/open-error/" + std::string(std::strerror(errno)));
        }

        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        if (file_size < pos + len) {
          throw jsi::JSError(runtime, "leveldbReadFileBuf/invalid-len-plus-pos");
        }

        file.seekg(pos, std::ios::beg);

        jsi::Function arrayBufferCtor = runtime.global().getPropertyAsFunction(runtime, "ArrayBuffer");
        jsi::Object o = arrayBufferCtor.callAsConstructor(runtime, len).getObject(runtime);
        jsi::ArrayBuffer buf = o.getArrayBuffer(runtime);
        if (!file.read((char*)buf.data(runtime), len)) {
          throw jsi::JSError(runtime, "leveldbReadFileBuf/read-error/" + std::string(std::strerror(errno)));
        }

        return o;
      }
  );
    jsiRuntime.global().setProperty(jsiRuntime, "leveldbReadFileBuf", std::move(leveldbReadFileBuf));
}

void cleanupLeveldb() {
  iterators.clear();
  dbs.clear();
}


