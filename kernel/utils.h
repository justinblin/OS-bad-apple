#pragma once

#include <stdint.h>
#include "shared.h"
#include "debug.h"
#include "heap.h"
#include "libk.h"
#include "buffer.h"

namespace impl::utils {

    class StringImpl {
    public:
        const size_t length;
        StringImpl(size_t length): length{length} {}
        virtual ~StringImpl() {};
        virtual char at(size_t index) const = 0;
        virtual StrongPtr<impl::ext2::Buffer<char>> toC() const {
            auto buffer = StrongPtr<impl::ext2::Buffer<char>>::make(length+1);
            for (size_t i=0; i<length; i++) {
                buffer->data[i] = at(i);
            }
            buffer->data[length] = 0;
            return buffer;
        }
    };


    const char* dup(const char* from, const size_t len) {
        if (len != 0) {
            auto p = (char*) malloc(len);
            ASSERT(p != nullptr);
            memcpy(p, from, len);
            return p;
        } else {
            return nullptr;
        }
    }

    class EmptyString: public StringImpl {
    public:
        EmptyString(): StringImpl(0) {
        }
        char at(size_t index) const override {
            Debug::panic("EmptyString::at(%d)", index);
        }
    };

    class Simple: public StringImpl {
        char const * const buffer;
    public:
        Simple(const char* in, size_t length): StringImpl(length), buffer(dup(in, length)) {   
        }
        ~Simple() {
            if (buffer != nullptr) {
                free((void*)buffer);
            }
        }
        char at(size_t index) const override {
            ASSERT(index < length);
            return buffer[index];
        }
    };

    class Concat: public StringImpl {
        StrongPtr<StringImpl> const lhs;
        StrongPtr<StringImpl> const rhs;
    public:
        Concat(StrongPtr<StringImpl> lhs, StrongPtr<StringImpl> rhs): StringImpl(lhs->length+rhs->length), lhs(lhs), rhs(rhs) {
        }
        char at(size_t index) const override {
            ASSERT(index < length);
            if (index < lhs->length) {
                return lhs->at(index);
            } else {
                return rhs->at(index - lhs->length);
            }
        }
    };

    class Replace: public StringImpl {
        StrongPtr<StringImpl> const base;
        const char from;
        const char to;
    public:
        Replace(StrongPtr<StringImpl> base, char from, char to) : StringImpl(base->length), base(base), from(from), to(to) {

        }
        char at(size_t index) const override {
            ASSERT(index < length);
            char ch = base->at(index);
            if (ch == from) {
                return to;
            } else {
                return ch;
            }
        }
    };

    class Slice: public StringImpl {
        StrongPtr<StringImpl> base;
        size_t from;
        size_t to;
    public:
        Slice(StrongPtr<StringImpl> base, size_t from, size_t to): StringImpl(to-from+1), base(base), from(from), to(to) {
            ASSERT(length <= base->length);
            ASSERT(from <= base->length);
            ASSERT(to <= base->length);
        }
        char at(size_t index) const override {
            ASSERT(index < length);
            return base->at(index + from);
        }
    };

    class String {
        StrongPtr<StringImpl> impl;
        String(StrongPtr<StringImpl> impl): impl(impl) {}
    public:
        String(): impl(StrongPtr<StringImpl>(new EmptyString())) {
        }

        explicit String(const char* cs): impl(StrongPtr<StringImpl>(new Simple(cs, K::strlen(cs)))) {
        }

        String(const char* cs, size_t len): impl(StrongPtr<StringImpl>(new Simple(cs,len))) {
        }

        inline size_t length() const {
            return impl->length;
        }

        inline String operator+(String rhs) const {
            if (length() == 0) {
                return rhs;
            } else if (rhs.length() == 0) {
                return *this;
            } else {
                return String(StrongPtr<StringImpl>(new Concat(impl, rhs.impl)));
            }
        }

        inline String operator+(const char* s) const {
            return (*this) + String{s};
        }

        inline String replace(char from, char to) const {
            if (from == to) {
                return String(impl);
            } else {
                return String(StrongPtr<StringImpl>(new Replace(impl, from , to)));
            }
        }

        inline String slice(size_t from, size_t to) const {
            if ((from == 0) && (to == length() - 1)) {
                return String(impl);
            }
            if (from > to) {
                return String("",0);
            }
            return String(StrongPtr<StringImpl>(new Slice(impl, from, to)));
        }

        template <typename Work>
        String drop_while(Work const& work) {
            auto len = length();
            for (size_t i=0; i<len; i++) {
                auto ch = impl->at(i);
                if (!work(ch)) return slice(i, len-1);
            }
            return String();
        }

        template <typename Work>
        inline String keep_while(Work const& work) {
            for (size_t i=0; i<length(); i++) {
                auto ch = impl->at(i);
                if (!work(ch)) return (i==0) ? String() : slice(0, i-1);
            }
            return String(impl);
        }

        inline bool starts_with(const char s) {
            return (length() > 0) && (impl->at(0) == s);
        }

        inline char at(size_t index) const {
            return impl->at(index);
        }

        inline StrongPtr<impl::ext2::Buffer<char>> toC() const {
            return impl->toC();
        }
        
    };

    
}