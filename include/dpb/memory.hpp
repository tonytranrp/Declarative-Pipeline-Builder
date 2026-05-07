#pragma once

#include <vector>
#include <memory_resource>
#include <type_traits>
#include <cstddef>

namespace dpb {

template<typename T>
class collect_buffer {
private:
    // Determine if we should use PMR optimization
    static constexpr bool use_pmr = std::is_trivially_copyable_v<T> && sizeof(T) <= 64;
    
    // PMR implementation
    struct pmr_impl {
        alignas(64) std::byte buffer[4096];  // 4KB stack buffer
        std::pmr::monotonic_buffer_resource resource;
        std::pmr::vector<T> vec;
        
        pmr_impl(size_t initial_capacity) 
            : resource(buffer, sizeof(buffer))
            , vec(&resource) 
        {
            if (initial_capacity > 0) {
                vec.reserve(initial_capacity);
            }
        }
        
        // Move constructor
        pmr_impl(pmr_impl&& other) noexcept
            : resource(buffer, sizeof(buffer))
            , vec(std::move(other.vec), &resource)
        {
            // Move the vector but keep our resource
        }
        
        // Disable copy
        pmr_impl(const pmr_impl&) = delete;
        pmr_impl& operator=(const pmr_impl&) = delete;
        pmr_impl& operator=(pmr_impl&&) = delete;
    };
    
    // Regular vector implementation
    struct std_impl {
        std::vector<T> vec;
        
        std_impl(size_t initial_capacity) {
            if (initial_capacity > 0) {
                vec.reserve(initial_capacity);
            }
        }
    };
    
    // Storage - using conditional type
    using storage_type = std::conditional_t<use_pmr, pmr_impl, std_impl>;
    storage_type impl_;
    
public:
    // Constructor with optional initial capacity
    explicit collect_buffer(size_t initial_capacity = 0) 
        : impl_(initial_capacity) 
    {}
    
    // Move constructor
    collect_buffer(collect_buffer&& other) noexcept 
        : impl_(std::move(other.impl_)) 
    {}
    
    // Disable copy
    collect_buffer(const collect_buffer&) = delete;
    collect_buffer& operator=(const collect_buffer&) = delete;
    collect_buffer& operator=(collect_buffer&&) = delete;
    
    // Vector-like interface
    void push_back(const T& value) {
        if constexpr (use_pmr) {
            impl_.vec.push_back(value);
        } else {
            impl_.vec.push_back(value);
        }
    }
    
    void push_back(T&& value) {
        if constexpr (use_pmr) {
            impl_.vec.push_back(std::move(value));
        } else {
            impl_.vec.push_back(std::move(value));
        }
    }
    
    template<typename... Args>
    void emplace_back(Args&&... args) {
        if constexpr (use_pmr) {
            impl_.vec.emplace_back(std::forward<Args>(args)...);
        } else {
            impl_.vec.emplace_back(std::forward<Args>(args)...);
        }
    }
    
    void reserve(size_t new_capacity) {
        if constexpr (use_pmr) {
            impl_.vec.reserve(new_capacity);
        } else {
            impl_.vec.reserve(new_capacity);
        }
    }
    
    size_t size() const noexcept {
        if constexpr (use_pmr) {
            return impl_.vec.size();
        } else {
            return impl_.vec.size();
        }
    }
    
    bool empty() const noexcept {
        if constexpr (use_pmr) {
            return impl_.vec.empty();
        } else {
            return impl_.vec.empty();
        }
    }
    
    auto begin() noexcept {
        if constexpr (use_pmr) {
            return impl_.vec.begin();
        } else {
            return impl_.vec.begin();
        }
    }
    
    auto end() noexcept {
        if constexpr (use_pmr) {
            return impl_.vec.end();
        } else {
            return impl_.vec.end();
        }
    }
    
    auto begin() const noexcept {
        if constexpr (use_pmr) {
            return impl_.vec.begin();
        } else {
            return impl_.vec.begin();
        }
    }
    
    auto end() const noexcept {
        if constexpr (use_pmr) {
            return impl_.vec.end();
        } else {
            return impl_.vec.end();
        }
    }
    
    auto data() noexcept {
        if constexpr (use_pmr) {
            return impl_.vec.data();
        } else {
            return impl_.vec.data();
        }
    }
    
    auto data() const noexcept {
        if constexpr (use_pmr) {
            return impl_.vec.data();
        } else {
            return impl_.vec.data();
        }
    }
    
    // Conversion to std::vector<T> (move)
    operator std::vector<T>() && {
        if constexpr (use_pmr) {
            // For PMR vector, we need to copy to regular vector
            std::vector<T> result;
            result.reserve(impl_.vec.size());
            result.insert(result.end(), impl_.vec.begin(), impl_.vec.end());
            return result;
        } else {
            // For regular vector, just move it
            return std::move(impl_.vec);
        }
    }
};

} // namespace dpb