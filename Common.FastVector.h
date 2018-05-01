//----------------------------------------------------------------------------
//  Author:     Dwayne Robinson
//  History:    2018-04-30 Created
//----------------------------------------------------------------------------
#pragma once


#if USE_MODULES
import Common.ArrayRef;
#else
#include "Common.ArrayRef.h"
#endif


// See fast_vector below for the common case which includes a fixed size array.
// The base class is separated out for the possibility of associating specific
// default memory, and also to avoid bloating additional template permutations
// solely due to differing array sizes.

template<typename T, size_t DefaultArraySize = 0, bool ShouldInitializeElements = true>
class fast_vector;

template<typename T, bool ShouldInitializeElements>
class fast_vector<T, 0, ShouldInitializeElements>
{
    static_assert(ShouldInitializeElements || std::is_trivial<T>::value);

public:
    // Types
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using iterator = pointer;
    using const_reference = T const&;
    using const_iterator = T const*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    enum use_memory_buffer_enum
    {
        use_memory_buffer
    };

public:
    fast_vector()
    {
    }

    fast_vector(use_memory_buffer_enum, array_ref<T> initialBackingArray)
    :   data_(initialBackingArray.data()),
        capacity_(std::size(initialBackingArray))
    {
    }

    fast_vector(size_t initialSize)
    {
        resize(initialSize);
    }

    fast_vector(array_ref<const T> initialValues)
    {
        assign(initialValues);
    }

    fast_vector(array_ref<const T> initialValues, use_memory_buffer_enum, array_ref<T> initialBackingArray)
    :   data_(initialBackingArray.data()),
        capacity_(std::size(initialBackingArray))
    {
        assign(initialValues);
    }

    fast_vector(const fast_vector& otherVector)
    {
        assign(otherVector);
    }

    // Disable move assignment and move constructor, since we cannot safely move values from one
    // to another without throwing, as the target vector may have a smaller fixed size and incur
    // a memory allocation which cannot be satisfied.
    fast_vector(fast_vector&& otherVector) = delete;
    fast_vector& operator=(fast_vector&& otherVector) = delete;

    fast_vector& operator=(const fast_vector& otherVector)
    {
        assign(otherVector);
        return *this;
    }

    ~fast_vector()
    {
        // Free any elements, whether in the fixed size array or heap array.
        if (ShouldInitializeElements)
        {
            std::destroy(data_, data_ + size_);
        }

        if (dataIsAllocatedMemory_)
        {
            free(data_);
        }
    }

    // Iterators
    iterator begin() const noexcept                 { return data_; }
    iterator end() const noexcept                   { return data_ + size_; }
    const_iterator cbegin() const noexcept          { return begin(); }
    const_iterator cend() const noexcept            { return end(); };
    reverse_iterator rbegin() const noexcept        { return reverse_iterator(begin()); }
    reverse_iterator rend() const noexcept          { return reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept   { return const_reverse_iterator(end()); }

    // Capacity
    size_type size() const noexcept                 { return size_; }
    size_type size_in_bytes() const noexcept        { return size_ * sizeof(T); }
    size_type capacity() const noexcept             { return capacity_; }
    static constexpr size_type max_size() noexcept  { return SIZE_MAX / sizeof(T); }
    bool empty() const noexcept                     { return size_ == 0; }

    // Element access
    T& operator[](size_t i) const noexcept          { return data_[i]; }
    T& front() const noexcept                       { return *data_; }
    T& back() const noexcept                        { return *(data_ + size_ - 1); }
    T* data() const noexcept                        { return data_; }
    T* data_end() const noexcept                    { return data_ + size_; }

    void assign(array_ref<const T> span)
    {
        clear();
        size_t newSize = span.size();
        reserve(newSize);
        std::uninitialized_copy(span.data(), span.data() + span.size(), /*out*/ data());
        size_ = newSize;
    }

    void clear()
    {
        std::destroy(data_, data_ + size_);
        size_ = 0;
        // But do not free heap memory.
    }

    void resize(size_t newSize)
    {
        if (newSize > size_)
        {
            if (newSize > capacity_)
            {
                // Grow with 1.5x factor to avoid frequent reallocations.
                size_t newCapacity = std::max((size_ * 3 / 2), newSize);
                reserve(newCapacity);
            }

            // Grow data to the new size.
            if (ShouldInitializeElements)
            {
                std::uninitialized_value_construct<iterator>(data_ + size_, data_ + newSize);
            }

            size_ = newSize;
        }
        else if (newSize < size_)
        {
            // Shrink the data to the new size. Capacity remains intact.
            if (ShouldInitializeElements)
            {
                std::destroy(data_ + newSize, data_ + size_);
            }

            size_ = newSize;
        }
    }

    void reserve(size_t newCapacity)
    {
        if (newCapacity <= capacity_)
        {
            return; // Nothing to do.
        }

        if (newCapacity > max_size())
        {
            throw std::bad_alloc(); // Too many elements.
        }

        size_t newByteSize = newCapacity * sizeof(T);
        ReallocateMemory(newByteSize);

        capacity_ = newCapacity;
    }

    void shrink_to_fit()
    {
        if (!dataIsAllocatedMemory_ || capacity_ == size_)
        {
            return; // Nothing to do.
        }

        size_t newByteSize = size_ * sizeof(T);
        ReallocateMemory(newByteSize);

        capacity_ = size_;
    }

    void push_back(const T& newValue)
    {
        reserve(size_ + 1);
        new(&data_[size_]) T(newValue);
        ++size_;
    }

    void push_back(T&& newValue)
    {
        reserve(size_ + 1);
        new(&data_[size_]) T(std::move(newValue));
        ++size_;
    }

protected:
    void ReallocateMemory(size_t newByteSize)
    {
        assert(newByteSize >= size_ * sizeof(T));
        newByteSize = std::max(newByteSize, size_t(1)); // Ensure at least one byte is allocated to avoid nullptr.

        if (dataIsAllocatedMemory_ && std::is_trivially_move_constructible<T>::value)
        {
            // Try to just reallocate the existing memory block.

            T* newData = static_cast<T*>(realloc(data_, newByteSize));
            if (newData == nullptr)
            {
                throw std::bad_alloc();
            }
            data_ = newData;
        }
        else
        {
            // Allocate a new memory buffer if one isn't allocated yet,
            // or if the data type is complex enough that it's not trivially
            // moveable (e.g. std::string, which contains pointers that point
            // into the class address itself for the small string optimization).

            T* newData = static_cast<T*>(malloc(newByteSize));
            std::unique_ptr<T, decltype(std::free)*> newDataHolder(newData, &std::free);

            // Copy an any existing elements from fixed size buffer.
            std::uninitialized_move(data_, data_ + size_, /*out*/newData);

            // Release the existing block, and assign the new one.
            if (dataIsAllocatedMemory_)
            {
                free(data_);
            }
            data_ = newDataHolder.release();
            dataIsAllocatedMemory_ = true;
        }
    }

protected:
    T* data_ = nullptr; // May point to fixed size array or allocated memory.
    size_t size_ = 0;
    size_t capacity_ = 0;
    bool dataIsAllocatedMemory_ = false;
};


// A lightweight dynamic array that mostly follows the interface of std vector, except that it
// (1) avoids heap allocation so long as the number of elements fits within the minimum stack size.
// (2) does not initialize memory for simple types, if ShouldInitializeElements == false.
// This avoids unnecessarily touching memory which will just be overwritten again later anyway.
//
template<typename T, size_t DefaultArraySize, bool ShouldInitializeElements>
//template <typename T, size_t DefaultArraySize, bool ShouldInitializeElements = true>
class fast_vector : public fast_vector<T, 0, ShouldInitializeElements>
{
public:
    using BaseClass = fast_vector<T, 0, ShouldInitializeElements>;

    fast_vector()
    :   BaseClass(use_memory_buffer, GetArrayData())
    {
    }

    fast_vector(array_ref<T> initialValues)
    :   BaseClass(initialValues, use_memory_buffer, GetArrayData())
    {
    }

    fast_vector(const fast_vector& otherVector)
    :   BaseClass(otherVector)
    {
    }

    // Disable move assignment and move constructor, since we cannot safely move values from one
    // to another without throwing, as the target vector may have a smaller fixed size and incur
    // a memory allocation which cannot be satisfied.
    fast_vector(fast_vector&& otherVector) = delete;
    fast_vector& operator=(fast_vector&& otherVector) = delete;

    fast_vector& operator=(const fast_vector& otherVector)
    {
        assign(otherVector);
        return *this;
    }

    fast_vector& operator=(const BaseClass& otherVector)
    {
        assign(otherVector);
        return *this;
    }

private:
    constexpr array_ref<T> GetArrayData() noexcept
    {
        return array_ref<T>(reinterpret_cast<T*>(std::data(arrayData_)), std::size(arrayData_));
    }

private:
    // Uninitialized data to be used by the base class.
    // It's declared as raw bytes rather than an std::array<T> to avoid any initialization cost
    // up front, only initializing the fields which actually exist when resized later.
    uint8_t arrayData_[DefaultArraySize][sizeof(T)];
};