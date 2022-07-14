#include "packer.h"

using namespace::facebook;

void pack(const jsi::Value& value, jsi::Runtime& runtime, mpack_writer_t* writer) {
  if(value.isString()) {
    
    mpack_write_utf8_cstr(writer, value.getString(runtime).utf8(runtime).c_str());
    
  } else if(value.isNumber()) {
    
    mpack_write(writer, value.getNumber());
    
  } else if(value.isBool()) {
    
    mpack_write(writer, value.getBool());
    
  } else if(value.isNull()) {
    
    mpack_write_nil(writer);
    
  } else if(value.isUndefined()){
    
    mpack_start_bin(writer, 1);
    // we need to put something in the bin
    mpack_write_bytes(writer, "u", 1);
    mpack_finish_bin(writer);
    
  } else if(value.isObject()) {
    
    auto obj = value.asObject(runtime);

    if(obj.isArray(runtime)) {
    
      auto array = obj.getArray(runtime);
      auto size = array.size(runtime);
      mpack_start_array(writer, size);
      
      for(size_t i=0 ; i<size; i++) {
        
        jsi::Value value = array.getValueAtIndex(runtime, i);
        pack(value, runtime, writer);
        
      }
      
      mpack_finish_array(writer);
      
    } else if(obj.isFunction(runtime)) {
      // noop
    } else if(obj.isArrayBuffer(runtime)) {
      // noop for now
    } else {
      
      // normal object
      auto names = obj.getPropertyNames(runtime);
      auto size = names.size(runtime);
      mpack_start_map(writer, size);
      
      for (size_t i=0;i < size; i++) {
        
        auto key = names.getValueAtIndex(runtime, i).asString(runtime);
        auto val = obj.getProperty(runtime, key);
        mpack_write_utf8_cstr(writer, key.utf8(runtime).c_str());
        pack(val, runtime, writer);
        
      }
      
      mpack_finish_map(writer);
    }
  }
}

jsi::Value unpackElement(jsi::Runtime& runtime, mpack_reader_t* reader, int depth) {
    if (depth >= 32) { // critical check!
        mpack_reader_flag_error(reader, mpack_error_too_big);
        return nullptr;
    }

    mpack_tag_t tag = mpack_read_tag(reader);
    if (mpack_reader_error(reader) != mpack_ok)
        return nullptr;

    switch (mpack_tag_type(&tag)) {
        
        case mpack_type_nil:
        
            return jsi::Value(nullptr);
        
        case mpack_type_bin: {
          
            // we only use bin for undefined so far
            uint32_t length = mpack_tag_bin_length(&tag);
            mpack_read_bytes_inplace(reader, length);
            mpack_done_bin(reader);
            return jsi::Value();
          
        }
        case mpack_type_bool:
        
            return jsi::Value(mpack_tag_bool_value(&tag));
        
        case mpack_type_double:
        
            return jsi::Value(mpack_tag_double_value(&tag));
        
        case mpack_type_str: {
          
            uint32_t length = mpack_tag_str_length(&tag);
            const char* data = mpack_read_utf8_inplace(reader, length);
            auto val = jsi::String::createFromUtf8(runtime, (uint8_t *) data, length);
            mpack_done_str(reader);
            return val;
        }

        case mpack_type_array: {
          
            uint32_t count = mpack_tag_array_count(&tag);
            auto array = jsi::Array(runtime, count);
            while (count-- > 0) {
                if (mpack_reader_error(reader) != mpack_ok) // critical check!
                    break;
                array.setValueAtIndex(runtime, count, unpackElement(runtime, reader, depth + 1));
            }
            mpack_done_array(reader);
            return array;
        }

        case mpack_type_map: {
          
            uint32_t count = mpack_tag_map_count(&tag);
            auto object = jsi::Object(runtime);

            while (count-- > 0) {
                auto key = unpackElement(runtime, reader, depth + 1);
                auto value = unpackElement(runtime, reader, depth + 1);
                if (mpack_reader_error(reader) != mpack_ok) // critical check!
                    break;
                object.setProperty(runtime, key.asString(runtime), value);
            }
            mpack_done_map(reader);
            return object;
          
        }
     
        default:
        
            mpack_reader_flag_error(reader, mpack_error_unsupported);
            return nullptr;
    }
}

