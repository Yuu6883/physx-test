#pragma once

#include <thread>
#include <memory.h>
#include <string_view>

using std::string_view;

static thread_local std::unique_ptr<char[]> pool(new char[10 * 1024 * 1024]);

class Writer {
    char* ptr;
public:
    Writer() : ptr(pool.get()) {};
    template<typename I>
    I& ref(I init = 0) {
        I& r = *((I*)ptr);
        r = 0;
        ptr += sizeof(I);
        return r;
    }
    template<typename I, typename O>
    void write(O&& input) {
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
    string_view buffer() {
        return string_view(pool.get(), ptr - pool.get());
    }
};