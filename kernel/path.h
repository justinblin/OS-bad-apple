#pragma once

#include "debug.h"
#include "shared.h"

class PathString {
private:
    StrongPtr<PathString> next{};

    char* name;
    uint32_t size;
    uint32_t max_size;

public:
    PathString() {
        size = 0;
        max_size = 10;
        name = new char[max_size];
    }

    ~PathString() {
        delete[] name;
    }

    void add_char(char c) {
        if(size + 1 >= max_size) {
            max_size *= 1.5;
            char* new_name = new char[max_size];
            for(uint32_t i = 0; i < size; i++) {
                new_name[i] = name[i];
            }
            delete[] name;
            name = new_name;
        }
        name[size++] = c;
    }

    char* get() {
        char* new_name = new char[size + 1];
        for(uint32_t i = 0; i < size; i++) {
            new_name[i] = name[i];
        }
        new_name[size] = '\0';
        return new_name;
    }

    bool isEmpty() {
        return size == 0;
    }

    friend class Path;
};

class Path {
private:
    StrongPtr<PathString> first{};
    StrongPtr<PathString> last{};

    void add(StrongPtr<PathString> path_string) {
        ASSERT(!(path_string == nullptr));
        path_string->next = StrongPtr<PathString>{};
        if (first == nullptr) {
            first = path_string;
        } else {
            last->next = path_string;
        }
        last = path_string;
    }

public:

    void clear() {
        first = nullptr;
        last = nullptr;
    }

    bool isEmpty() {
        return first == nullptr;
    }

    StrongPtr<PathString> remove() {
        if (first == nullptr) {
            return StrongPtr<PathString>();
        }
        auto it = first;
        first = it->next;
        if (first == nullptr) {
            last = nullptr;
        }
        return it;
    }

    void merge_symbolic_link(Path path) {
        if(path.first == nullptr || path.last == nullptr) {
            return;
        }
        path.last->next = first;
        first = path.first;
    }

    Path(const char* path) {
        uint32_t i = 0;

        // if empty path
        if(path[i] == '\0') return;

        // took care of root
        if(path[i] == '/') {
            StrongPtr<PathString> path_string = StrongPtr<PathString>::make();
            path_string->add_char('/');
            add(path_string);
        }
        while(path[i] == '/') i++;

        // parse rest
        StrongPtr<PathString> path_string = StrongPtr<PathString>::make();
        while(path[i] != '\0') {
            if(path[i] == '/') {
                add(path_string);
                path_string = StrongPtr<PathString>::make();
                while(path[++i] == '/'){};
            } else {
                path_string->add_char(path[i++]);
            }
        }
        if(!path_string->isEmpty()) {
            add(path_string);
        }
    }

    ~Path() {
        while(!(remove() == nullptr)){};
    }
};