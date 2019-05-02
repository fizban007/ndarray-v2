# Introduction
Header-only implementation of an N-dimensional array for modern C++.


## Overview
This library adopts an abstract notion of an array. An array is any mapping from a space of N-dimensional indexes `(i, j, ...)` to some type of value, and which defines a rectangular region (the shape) containing the valid indexes:

    `array: (index => value, shape)`

This means that the mapped values may be generated in a variety of ways - either by looking them up in a memory buffer (as in a conventional array), or by calling a function with the index as a parameter.

An array is a class template parameterized around a `provider` type. Operations applied to the array (such as slicing, reshaping, or mapping the underlying values) generate a provider of a new type, and return an array holding that new provider. This enables a highly optimized and memory-efficient code to be generated by the compiler. Arrays can be converted to memory-backed arrays at any stage in an algorithm. This caches their values and collapses the type hierarchy.


## Immutability
Arrays are immutable, meaning that you manipulate them by applying transformations to existing arrays to generate new ones. But they are also light-weight objects that incur essentially zero overhead when passed by value. This is because memory-backed arrays only hold a `std::shared_ptr` to an immutable memory buffer, while operated-on arrays only hold lightweight function objects. Transformed arrays do not allocate new memory buffers, and do not perform any calculations until they are indexed, or converted to a memory-backed array. Such lazy evaluation trades compile time overhead in exchange for runtime performace (the compiler sees the whole type hierarchy and can scrunch it down to perform optimizations) and reduced memory footprint.

There is one exception to immutability, a `unique_array`, which is memory-backed and read/write, so it enables procedural loading of data into a memory-backed array. The `unique_array` owns its data buffer, and is move-constructible but not copy-constructible (following the semantics of `unique_ptr`). After loading data into it, it can be moved to a shared (immutable, copy-constructible) memory-backed array. The fact that mutable arrays cannot be copied ensures that you're not accidentally passing around heavyweight objects by value.


## Quick-start
Create a 10 x 20 array of zero-initialized ints:
```C++
auto A = nd::zeros(10, 20);
```

Add an array of doubles to it:
```C++
auto B = A + nd::ones<double>(10, 20);
```

Get the shape and total number of elements in an array:
```C++
auto shape = A.shape();
auto size = A.size();
```

Create an array as a subset of another:
```C++
auto B = A | nd::select_from(0, 0).to(10, 10).jumping(2, 2); // B.shape() == {5, 5}
```

Create an array by substituting a region with values from another array:
```C++
auto B = A | nd::replace_from(0, 0).to(10, 5).with(nd::zeros(10, 5));
```

Transform an array element-wise
```C++
auto B = A | nd::transform([] (auto x) { return x * x; });
```
