#pragma once

#include <stereo/defs.h>

namespace stereo {

template <typename T>
struct UniformBox {
    T value;
    
    UniformBox() = default;
    UniformBox(const T& value): value(value) {}
    UniformBox(T&& value):      value(std::move(value)) {}
    
    operator       T&()       { return value; }
    operator const T&() const { return value; }
    
          T* operator->()       { return &value; }
    const T* operator->() const { return &value; }
    
          T* operator*()       { return &value; }
    const T* operator*() const { return &value; }
    
    UniformBox& operator=(const T& value) {
        this->value = value;
        return *this;
    }
    
    UniformBox& operator=(T&& value) {
        this->value = std::move(value);
        return *this;
    }
private:
    
    uint8_t _padding[
          (sizeof(T) % MIN_UNIFORM_PADDING_BYTES > 0) * MIN_UNIFORM_PADDING_BYTES
        - (sizeof(T) % MIN_UNIFORM_PADDING_BYTES)
    ];
    
    // static_assert(
    //     sizeof(UniformBox) % MIN_UNIFORM_PADDING_BYTES == 0,
    //     "UniformBox must be a multiple of MIN_UNIFORM_PADDING_BYTES"
    // );
};

}  // namespace stereo
