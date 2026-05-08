#pragma once

#include <vector>
#include <cstddef>
#include <utility>
#include <memory>

namespace dpb {

template<typename T, typename Alloc = std::allocator<T>>
class collect_buffer {
private:
    std::vector<T, Alloc> vec_;
    
public:
    explicit collect_buffer(size_t initial_capacity = 0, const Alloc& alloc = Alloc()) 
        : vec_(alloc)
    {
        if (initial_capacity > 0) {
            vec_.reserve(initial_capacity);
        }
    }
    
    collect_buffer(collect_buffer&&) = default;
    collect_buffer(const collect_buffer&) = delete;
    collect_buffer& operator=(const collect_buffer&) = delete;
    collect_buffer& operator=(collect_buffer&&) = delete;

    void push_back(const T& value) {
        vec_.push_back(value);
    }
    
    void push_back(T&& value) {
        vec_.push_back(std::move(value));
    }
    
    template<typename... Args>
    void emplace_back(Args&&... args) {
        vec_.emplace_back(std::forward<Args>(args)...);
    }
    
    void reserve(size_t new_capacity) {
        vec_.reserve(new_capacity);
    }
    
    size_t size() const noexcept {
        return vec_.size();
    }
    
    bool empty() const noexcept {
        return vec_.empty();
    }
    
    auto begin() noexcept {
        return vec_.begin();
    }
    
    auto end() noexcept {
        return vec_.end();
    }
    
    auto begin() const noexcept {
        return vec_.begin();
    }
    
    auto end() const noexcept {
        return vec_.end();
    }
    
    auto data() noexcept {
        return vec_.data();
    }
    
    auto data() const noexcept {
        return vec_.data();
    }
    
    operator std::vector<T, Alloc>() && {
        return std::move(vec_);
    }
};

} // namespace dpb