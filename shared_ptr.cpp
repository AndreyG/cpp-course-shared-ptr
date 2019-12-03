#include <atomic>
#include <new>

struct control_block {
    virtual ~control_block() = default;
    virtual void destroy() = 0;

    std::atomic_size_t ref_count { 1 };
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

template<typename T, typename... Args>
shared_ptr<T> make_shared(Args &&...);

template<typename T>
class shared_ptr {
    T* ptr;
    control_block* cb;

public:
    template<typename D>
    shared_ptr(T* ptr, D deleter)
        : ptr(ptr)
        , cb(new control_block_impl<T, D>(ptr, deleter))
    {};

    template<typename U>
    shared_ptr(shared_ptr<U> const & other)
        : ptr(other.ptr)
        , cb(other.cb)
    {
        ++cb->ref_count;
    }

    ~shared_ptr() {
        if (--cb->ref_count == 0) {
            cb->destroy();
            delete cb;
        }
    }

private:
    template<typename U, typename... Args>
    friend shared_ptr<U> make_shared(Args &&...);

    shared_ptr(control_block_inplace<T>* cb)
        : ptr(cb->get())
        , cb(cb)
    {};
};

template<typename T, typename... Args>
shared_ptr<T> make_shared(Args &&... args) {
    return shared_ptr<T>(new control_block_inplace<T>(std::forward<Args>(args)...));
}