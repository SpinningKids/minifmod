
#pragma once

#include <cassert>
#include <utility>

template<typename T>
class simple_array
{
    size_t size_;
    T* data_;
public:
    simple_array() : size_{ 0 }, data_{} {}
    simple_array(size_t size) : size_{ size }, data_{ new T[size]{} } {}
    simple_array(simple_array&& other) noexcept : size_{ std::exchange(other.size_, 0) }, data_{ std::exchange(other.data_, nullptr) } {}

    T* begin() { return data_; }
    const T* begin() const { return data_; }
    const T* cbegin() const { return data_; }

    T* end() { return data_ + size_; }
    const T* end() const { return data_ + size_; }
    const T* cend() const { return data_ + size_; }

    T& operator[] (size_t pos) { return data_[pos]; }
    const T& operator[] (size_t pos) const { return data_[pos]; }

    T* data() { return data_; }
    const T* data() const { return data_; }
    const T* cdata() const { return data_; }

    size_t size() const { return size_; }

    void resize(size_t size)
    {
        assert(!data_);
        data_ = new T[size]{};
        size_ = size;
    }

    ~simple_array()
    {
        delete[] data_;
    }
};