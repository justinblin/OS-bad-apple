#pragma once

#include "debug.h"
#include "atomic.h"

namespace impl::shared {
    template <typename T>
    struct Intermediate {
        T* volatile obj;
        SpinLock lock;
        volatile uint32_t strong_count;
        volatile uint32_t weak_count;

        Intermediate(T* obj): obj(obj), lock{}, strong_count{1}, weak_count{0} {

        }
    };

    template <typename T>
    [[nodiscard]]
    Intermediate<T>* add_strong(Intermediate<T>* ptr) {
        if (ptr != nullptr) {
            ptr->lock.lock();
            if (ptr->obj != nullptr) {
                ptr->strong_count += 1;
            }
            ptr->lock.unlock();
        }
        return ptr;
    }

    template <typename T>
    [[nodiscard]]
    Intermediate<T>* drop_strong(Intermediate<T>* ptr) {
        if (ptr != nullptr) {
            ptr->lock.lock();
            ASSERT(ptr->strong_count > 0);
            ASSERT(ptr->obj != nullptr);
            ptr->strong_count -= 1;
            
            bool delete_ptr = false;
            bool delete_obj = false;

            if (ptr->strong_count == 0) {
                delete_obj = true;
                if (ptr->weak_count == 0) {
                    delete_ptr = true;
                }
            }
            ptr->lock.unlock();

            // delete_ptr -> delete_obj
            ASSERT((!delete_ptr) || delete_obj);

            if (delete_obj) {
                ASSERT(ptr->obj != nullptr);
                delete ptr->obj;
                ptr->obj = nullptr;
            }
            
            if (delete_ptr) {
                delete ptr;
                ptr = nullptr;
                return nullptr;
            }
        }
        return ptr;
    }

    template <typename T>
    [[nodiscard]]
    Intermediate<T>* add_weak(Intermediate<T>* ptr) {
        if (ptr != nullptr) {
            ptr->lock.lock();
            ASSERT((ptr->weak_count > 0) || (ptr->strong_count > 0));
            ptr->weak_count += 1;
            ptr->lock.unlock();
        }
        return ptr;
    }

    template <typename T>
    [[nodiscard]]
    Intermediate<T>* drop_weak(Intermediate<T>* ptr) {
        if (ptr != nullptr) {
            ptr->lock.lock();
            ASSERT(ptr->weak_count > 0);
            ptr->weak_count -= 1;
            auto delete_it = (ptr->weak_count == 0) && (ptr->strong_count == 0);
            ptr->lock.unlock();
            if (delete_it) {
                delete ptr;
                ptr = nullptr;
                return nullptr;
            }
        }
        return ptr;
    }

    template <typename T>
    [[nodiscard]]
    Intermediate<T>* promote_weak(Intermediate<T>* ptr) {
        if (ptr != nullptr) {
            ptr->lock.lock();
            ASSERT(ptr->weak_count > 0);
            if (ptr->strong_count > 0) {
                ptr->strong_count += 1;
                ptr->lock.unlock();
                return ptr;
            } else {
                ptr->lock.unlock();
                return nullptr;
            }
        }
        return ptr;
    }
}

template <typename T>
class WeakPtr;

template <typename T>
class StrongPtr {

    impl::shared::Intermediate<T>* ptr;

    StrongPtr(impl::shared::Intermediate<T>* ptr, bool cheat):  ptr(ptr) {}

    
public:
    // A null pointer
    // example:
    //     StrongPtr p{};
    //     p == nullptr; // --> true
    StrongPtr() {
        ptr = nullptr;
    }

    ~StrongPtr() {
        ptr = drop_strong(ptr);    
    }

    // Wraps p
    // example:
    //     StrongPtr p{q};
    //     p == q; // -> true
    explicit StrongPtr(T* p) {
        using namespace impl::shared;

        ptr = (p == nullptr)? nullptr: new Intermediate{p}; 
    }

    // Copy constructor
    // example:
    //     StrongPtr p{new ...};
    //     StrongPtr q{p};
    //     p == q; // -> true
    StrongPtr(StrongPtr<T> const& src): ptr{impl::shared::add_strong(src.ptr)} {}


    // Assignment operator -- StrongPtr
    // example:
    //    auto p = StrongPtr<Thing>::make(...);
    //    auto q = StrongPtr<Thing>::make(...);
    //    p == q; // false
    //    q = q;  // first object is deleted
    //    p == q; // true
    StrongPtr<T>& operator =(StrongPtr<T> const& rhs) {
        using namespace impl::shared;

        if (ptr != rhs.ptr) {
            auto will_be = add_strong(rhs.ptr);
            ptr = drop_strong(ptr);
            ptr = will_be;
        }
        return *this;
    }

    // Assignment opertor -- raw pointer
    // example:
    //     auto p = StrongPtr<Thing>::make(...);
    //     p = new Thing(...); // first object deleted, p refers to the new object
    StrongPtr<T>& operator =(T* rhs) {
        using namespace impl::shared;

        if (rhs == nullptr) {
            ptr = drop_strong(ptr);
            ptr = nullptr;
            return *this;
        }

        if ((ptr != nullptr) && (ptr->obj == rhs)) {
            return *this;
        }

        ptr = drop_strong(ptr);
        ptr = (rhs == nullptr) ? nullptr : new Intermediate(rhs);

        return *this;
    }

    // Equality - StringPtr
    bool operator ==(StrongPtr<T>const & rhs) const {
        return ptr == rhs.ptr;
    }

    bool operator !=(StrongPtr<T>const & rhs) const {
        return ptr != rhs.ptr;
    }

    // Equality -- null
    bool operator ==(nullptr_t rhs) const {
        return ptr == rhs;
    }

    bool operator !=(nullptr_t rhs) const {
        return ptr != rhs;
    }

    // arrow operator
    // example:
    //    auto p = StrongPtr<Thing>::make(...);
    //    p->do_something();
    T* operator->() const {
        ASSERT(ptr != nullptr);
        ASSERT(ptr->obj != nullptr);
        return ptr->obj;
    }

    // forwarding factory
    // example:
    //     auto p = StrongPtr<Thing>::make(...);
    //     /* same as StrongPtr<Thing> p { new Thing(...) }; */
    template <typename... Args>
    static StrongPtr<T> make(Args... args) {
        return StrongPtr<T>{new T(args...)};
    }

    friend class WeakPtr<T>;

};

template <typename T>
class WeakPtr {
    impl::shared::Intermediate<T>* ptr;


public:
    WeakPtr(const StrongPtr<T>& src): ptr(impl::shared::add_weak(src.ptr)) {
        
    }

    ~WeakPtr() {
        using namespace impl::shared;
        ptr = drop_weak(ptr);
    }

    WeakPtr(WeakPtr<T> const& src): ptr(impl::shared::add_weak(src.ptr)) {
    }

    WeakPtr& operator=(WeakPtr<T> const& rhs) {
        if (rhs.ptr == ptr) return *this;
        auto will_be = add_weak(rhs.ptr);
        ptr = drop_weak(ptr);
        ptr = will_be;
        return *this;
    }

    // the returned StrongPtr refers to:
    //     - the original object iff strong references to it still exist
    //     - nullptr iff strong references to the original object are all gone
    //
    // example:
    //    StrongPtr<Thing> p = StrongPtr<Thing>::make(...);
    //    WeakPtr<Thing> w{p};
    //    w.strong() == nullptr;  // false
    //    p = nullptr;
    //    w.strong() == nullptr;  // true
    StrongPtr<T> promote() const {
        auto p = impl::shared::promote_weak(ptr);
        return StrongPtr{p, true};
    }
};
