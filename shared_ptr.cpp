#include <atomic>

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
};
