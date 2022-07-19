#include "packer.h"

using namespace::facebook;

void Packer::pack(const jsi::Value& value, jsi::Runtime& runtime, mpack_writer_t* writer) {
    if(value.isString()) {
        mpack_write_cstr(writer, value.getString(runtime).utf8(runtime).c_str());
        
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
        
        auto obj = value.getObject(runtime);
        
        if(obj.isArray(runtime)) {
            
            auto array = obj.getArray(runtime);
            auto size = array.size(runtime);
            mpack_start_array(writer, size);
            
            for(size_t i=0; i<size; i++) {
                pack(array.getValueAtIndex(runtime, i), runtime, writer);
            }
            
            mpack_finish_array(writer);
            
        } else if(obj.isFunction(runtime)) {
            throw jsi::JSError(runtime, "pack/ functions are not supported");
        } else if(obj.isArrayBuffer(runtime)) {
            throw jsi::JSError(runtime, "pack/ ArrayBuffers are not supported");
        } else {
            
            // normal object
            auto keys = obj.getPropertyNames(runtime);
            auto keyCount = keys.size(runtime);
            mpack_start_map(writer, keyCount);
            
            for (size_t i=0;i < keyCount; i++) {
                // key
                auto key = keys.getValueAtIndex(runtime, i).getString(runtime);
                mpack_write_cstr(writer, key.utf8(runtime).c_str());
                // value
                pack(obj.getProperty(runtime, key), runtime, writer);                
            }
            
            mpack_finish_map(writer);
        }
    }
}

jsi::String unpackString(jsi::Runtime& runtime, mpack_reader_t* reader, size_t strLength) {
    if (mpack_should_read_bytes_inplace(reader, strLength)) {
        const char* data = mpack_read_bytes_inplace(reader, strLength);
        
        if (mpack_reader_error(reader) != mpack_ok)
            throw jsi::JSError(runtime, "unpackKey/ failed to read inplace");
                
        mpack_done_str(reader);
        return jsi::String::createFromUtf8(runtime,(uint8_t *) data, strLength);
    } else {
        char* data = (char *) malloc(strLength);
        mpack_read_bytes(reader, data, strLength);
        if (mpack_reader_error(reader) != mpack_ok) {
            free(data);
            throw jsi::JSError(runtime, "unpackKey/ failed to read with malloc");
        }
        auto key = jsi::String::createFromUtf8(runtime,(uint8_t *) data, strLength);
        free(data);
        mpack_done_str(reader);
        return key;
    }
}


jsi::Value Packer::unpackElement(jsi::Runtime& runtime, mpack_reader_t* reader, int depth) {
    if (depth >= 32) { // critical check!
        mpack_reader_flag_error(reader, mpack_error_too_big);
        throw jsi::JSError(runtime, "unpackElement/ maximum depth reached");
    }
    
    mpack_tag_t tag = mpack_read_tag(reader);
    
    if (mpack_reader_error(reader) != mpack_ok)
        throw jsi::JSError(runtime, "unpackElement/ failed to read tag");
    
    switch (mpack_tag_type(&tag)) {
            
        case mpack_type_nil:
            
            return jsi::Value(nullptr);
            
        case mpack_type_bin: {
            
            // We only use bin for undefined so far.
            // It is safe to read in place as we only store 1 byte
            size_t length = mpack_tag_bin_length(&tag);
            mpack_read_bytes_inplace(reader, length);
            mpack_done_bin(reader);
            return jsi::Value(); // undefined
            
        }
        case mpack_type_bool:
            
            return jsi::Value(mpack_tag_bool_value(&tag));
            
        case mpack_type_double:
            
            return jsi::Value(mpack_tag_double_value(&tag));
            
        case mpack_type_str: {
            
            size_t length = mpack_tag_str_length(&tag);
            return unpackString(runtime, reader, length);
        }
            
        case mpack_type_array: {
            
            size_t count = mpack_tag_array_count(&tag);
            jsi::Array array = jsi::Array(runtime, count);
            for(size_t i = 0; i< count; i++) {
                if (mpack_reader_error(reader) != mpack_ok) // critical check!
                    break;
                array.setValueAtIndex(runtime, i, unpackElement(runtime, reader, depth + 1));
            }
            mpack_done_array(reader);
            return array;
        }
            
        case mpack_type_map: {
            
            size_t count = mpack_tag_map_count(&tag);
            jsi::Object object = jsi::Object(runtime);
            
            for(size_t i = 0; i< count; i++) {
                auto keyLength = mpack_expect_str(reader);
                auto key = unpackString(runtime, reader, keyLength);
                auto value = unpackElement(runtime, reader, depth + 1);
                
                if (mpack_reader_error(reader) != mpack_ok) // critical check!
                    throw jsi::JSError(runtime, "unpackElement/ failed to read element for key");
                
                object.setProperty(runtime, key, value);
            }
            mpack_done_map(reader);
            return object;
            
        }
            
        default:
            
            mpack_reader_flag_error(reader, mpack_error_unsupported);
            throw jsi::JSError(runtime, "unpackElement/ unsupported element type");
    }
}

