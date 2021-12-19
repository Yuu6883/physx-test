#pragma once

#include <thread>
#include <memory.h>
#include <string_view>
#include <lz4.h>

using std::string_view;

// Placeholder
static thread_local std::unique_ptr<char[]> pool;

class Writer {
    char* ptr;
public:
    size_t offset() { return ptr - pool.get(); };
    
    Writer() {
        if (!pool.get()) pool.reset(new char[10 * 1024 * 1024]);
        ptr = pool.get();
    };

    template<typename I>
    inline I& ref(I init = 0) {
        I& r = *((I*)ptr);
        r = init;
        ptr += sizeof(I);
        return r;
    }

    template<typename I, typename O>
    inline void write(O&& input) {
        *((I*)ptr) = (I)input;
        ptr += sizeof(I);
    }

    void write(string_view buffer, bool padZero = true, bool asUTF16 = false) {
        if (asUTF16) {
            auto p = buffer.data();
            for (int i = 0; i < buffer.length(); i++)
                write<char16_t>(p[i]);
            if (padZero) write<char16_t>(0);
        }
        else {
            memcpy(ptr, buffer.data(), buffer.size());
            ptr += buffer.size();
            if (padZero) write<uint8_t>(0);
        }
    }

    void fill(uint8_t v, size_t size) {
        memset(ptr, v, size);
        ptr += size;
    }

    string_view finalize() {
        size_t s = ptr - pool.get();
        char* out = static_cast<char*>(malloc(s));
        memcpy(out, pool.get(), s);
        ptr = pool.get();
        return string_view(out, s);
    }

    string_view lz4() {
        size_t s = ptr - pool.get();
        int bound = LZ4_compressBound(s);
        char* out = static_cast<char*>(malloc(bound));
        int compressed = LZ4_compress_default(pool.get(), out, s, bound);

        if (compressed > 0) return string_view(out, compressed);
        else return string_view(nullptr, 0);
    }

    string_view buffer() {
        return string_view(pool.get(), ptr - pool.get());
    }
};