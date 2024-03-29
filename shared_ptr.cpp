﻿#include <atomic>
#include <new>
#include <type_traits>

struct control_block {
    virtual ~control_block() = default;
    virtual void destroy() = 0;

    std::atomic_size_t ref_count { 1 };
    std::atomic_size_t weak_ref_count { 0 };
};

template<typename T, typename D>
struct control_block_impl final : control_block {
    T* ptr;
    D deleter;

    void destroy() override {
        deleter(ptr);
    }
};

template<typename T>
struct control_block_inplace final : control_block {
    std::aligned_storage_t<sizeof(T), alignof(T)> storage;

    T* get() { return reinterpret_cast<T*>(&storage); }

    template<typename... Args>
    control_block_inplace(Args &&... args) {
        new(&storage) T(std::forward<Args>(args)...);
    }

    void destroy() override {
        get()->~T();
    }
};

template<typename T>
class shared_ptr;

template<typename T>
class weak_ptr;

template<typename T, typename... Args>
shared_ptr<T> make_shared(Args &&...);

template<typename T>
class enable_shared_from_this {
    weak_ptr<T> weak_ref;

public:
    shared_ptr<T> shared_from_this() {
        return weak_ref.lock();
    }
};

template<typename T>
class shared_ptr {
    T* ptr;
    control_block* cb;

public:
    template<typename D>
    shared_ptr(T* ptr, D deleter)
        : shared_ptr(ptr, new control_block_impl<T, D>(ptr, deleter))
    {};

    template<typename U>
    shared_ptr(shared_ptr<U> const & other)
        : shared_ptr(other.ptr, other.cb)
    {
        ++cb->ref_count;
    }

    ~shared_ptr() {
        if (--cb->ref_count == 0) {
            cb->destroy();
            if (cb->weak_ref_count == 0)
                delete cb;
        }
    }

    template<typename U> // reuse other control block
    shared_ptr(T* ptr, shared_ptr<U> const & other)
        : shared_ptr(ptr, other.cb)
    {
        ++cb->ref_count;
    }

private:
    friend weak_ptr<T>;

    template<typename U, typename... Args>
    friend shared_ptr<U> make_shared(Args &&...);

    shared_ptr(control_block_inplace<T>* cb)
        : shared_ptr(cb->get(), cb)
    {};

    shared_ptr(T* ptr, control_block* cb)
        : ptr(ptr)
        , cb(cb)
    {
        if constexpr (std::is_convertible_v<T*, enable_shared_from_this<T>*>) {
            static_cast<enable_shared_from_this<T>*>(ptr)->weak_ref = *this;
        }
    };
};

template<typename T>
class weak_ptr {
    T* ptr;
    control_block* cb;

    weak_ptr(T* ptr, control_block* cb)
        : ptr(ptr)
        , cb(cb)
    {
        ++cb->weak_ref_count;
    }

public:
    weak_ptr(shared_ptr<T> const & shared)
        : weak_ptr(shared.ptr, shared.cb)
    {}

    weak_ptr(weak_ptr const & other)
        : weak_ptr(other.ptr, other.cb)
    {}

    ~weak_ptr() {
        if (--cb->weak_ref_count == 0 && cb->ref_count == 0)
            delete cb;
    }

    shared_ptr<T> lock() {
        if (cb->ref_count++ == 0) {
            --cb->ref_count;
            return nullptr;
        }
        return { ptr, cb };
    }
};

template<typename T, typename... Args>
shared_ptr<T> make_shared(Args &&... args) {
    return shared_ptr<T>(new control_block_inplace<T>(std::forward<Args>(args)...));
}