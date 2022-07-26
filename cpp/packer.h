#ifndef packer_h
#define packer_h

#include <stdio.h>
#include <jsi/jsi.h>

#import "mpack.h"

using namespace facebook;
namespace Packer {
    jsi::Value unpackElement(jsi::Runtime& runtime, mpack_reader_t* reader, int depth);
    void pack(const jsi::Value& value, jsi::Runtime& runtime, mpack_writer_t* writer);
}
#endif /* packer_h */
