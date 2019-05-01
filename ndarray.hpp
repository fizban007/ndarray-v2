#pragma once
#include <algorithm>         // std::all_of
#include <functional>        // std::ref
#include <initializer_list>  // std::initializer_list
#include <iterator>          // std::distance
#include <numeric>           // std::accumulate
#include <utility>           // std::index_sequence
#include <string>            // std::to_string




/**
 * An N-dimensional array is an access pattern (sequence of index<N>), and a
 * provider (shape<N>, index<N> => value_type).
 *
 * The access pattern is based on a start, final, jumps scheme. It has begin/end
 * methods as well as a random-access method which generates an index into the
 * provider, given a source index. Access patterns can be transformed in
 * different ways - selected, shifted, and reshaped. A provider might be able to
 * be reshaped, but not necessarily, and the overlying array can only be
 * reshaped if its provider can. The provider might also offer transformations
 * on its index space, such as permuting axes (including transpositions).
 */




//=============================================================================
namespace nd
{


    // array support structs
    //=========================================================================
    template<std::size_t Rank, typename ValueType, typename DerivedType> class short_sequence_t;
    template<std::size_t Rank, typename Provider> class array_t;
    template<std::size_t Rank> class shape_t;
    template<std::size_t Rank> class index_t;
    template<std::size_t Rank> class jumps_t;
    template<std::size_t Rank> class memory_strides_t;
    template<std::size_t Rank> class access_pattern_t;
    template<typename ValueType> class buffer_t;


    // provider types
    //=========================================================================
    template<std::size_t Rank> class index_provider_t;
    template<std::size_t Rank, typename ValueType> class shared_provider_t;
    template<std::size_t Rank, typename ValueType> class unique_provider_t;
    template<std::size_t Rank, typename ValueType, typename ArrayTuple> class zipped_provider_t;


    // algorithm support structs
    //=========================================================================
    template<typename ValueType> class range_container_t;
    template<typename ValueType, typename ContainerTuple> class zipped_container_t;
    template<typename ContainerType, typename Function> class transformed_container_t;


    // access pattern factory functions
    //=========================================================================
    template<typename... Args> auto make_shape(Args... args);
    template<typename... Args> auto make_index(Args... args);
    template<typename... Args> auto make_jumps(Args... args);
    template<std::size_t Rank, typename Arg> auto make_uniform_shape(Arg arg);
    template<std::size_t Rank, typename Arg> auto make_uniform_index(Arg arg);
    template<std::size_t Rank, typename Arg> auto make_uniform_jumps(Arg arg);
    template<std::size_t Rank> auto make_strides_row_major(shape_t<Rank> shape);
    template<std::size_t Rank> auto make_access_pattern(shape_t<Rank> shape);
    template<typename... Args> auto make_access_pattern(Args... args);
    template<typename... Args> auto make_index_provider(Args... args);


    // provider factory functions
    //=========================================================================
    template<typename ValueType, std::size_t Rank> auto make_shared_provider(shape_t<Rank> shape);
    template<typename ValueType, typename... Args> auto make_shared_provider(Args... args);
    template<typename ValueType, std::size_t Rank> auto make_unique_provider(shape_t<Rank> shape);
    template<typename ValueType, typename... Args> auto make_unique_provider(Args... args);
    template<typename Provider, typename Accessor> auto evaluate_as_shared(Provider&&, Accessor&&);
    template<typename Provider, typename Accessor> auto evaluate_as_unique(Provider&&, Accessor&&);
    template<typename Provider> auto evaluate_as_shared(Provider&&);
    template<typename Provider> auto evaluate_as_unique(Provider&&);
    template<typename... ArrayTypes> auto zip_arrays(ArrayTypes&&... arrays);


    // array factory functions
    //=========================================================================
    template<typename Provider, typename Accessor> auto make_array(Provider&&, Accessor&&);
    template<typename Provider> auto make_array(Provider&&);


    // std::algorithm wrappers for ranges
    //=========================================================================
    template<typename Range, typename Seed, typename Function> auto accumulate(Range&& rng, Seed&& seed, Function&& fn);
    template<typename Range, typename Predicate> auto all_of(Range&& rng, Predicate&& pred);
    template<typename Range, typename Predicate> auto any_of(Range&& rng, Predicate&& pred);
    template<typename Range> auto distance(Range&& rng);
    template<typename Range> auto enumerate(Range&& rng);
    template<typename ValueType> auto range(ValueType count);
    template<typename... ContainerTypes> auto zip(ContainerTypes&&... containers);
}




//=============================================================================
template<typename ValueType>
class nd::range_container_t
{
public:

    using value_type = ValueType;

    //=========================================================================
    struct iterator
    {
        using iterator_category = std::input_iterator_tag;
        using value_type = ValueType;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator& operator++() { ++current; return *this; }
        bool operator==(const iterator& other) const { return current == other.current; }
        bool operator!=(const iterator& other) const { return current != other.current; }
        const ValueType& operator*() const { return current; }
        ValueType current = 0;
        ValueType start = 0;
        ValueType final = 0;
    };

    //=========================================================================
    range_container_t(ValueType start, ValueType final) : start(start), final(final) {}
    iterator begin() const { return { 0, start, final }; }
    iterator end() const { return { final, start, final }; }

    template<typename Function>
    auto operator|(Function&& fn) const
    {
        return transformed_container_t<range_container_t, Function>(*this, fn);
    }

private:
    //=========================================================================
    ValueType start = 0;
    ValueType final = 0;
};




//=============================================================================
template<typename ValueType, typename ContainerTuple>
class nd::zipped_container_t
{
public:

    using value_type = ValueType;

    //=========================================================================
    template<typename IteratorTuple>
    struct iterator
    {
        using iterator_category = std::input_iterator_tag;
        using value_type = value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator& operator++()
        {
            iterators = transform_tuple([] (auto x) { return ++x; }, iterators);
            return *this;
        }

        bool operator!=(const iterator& other) const
        {
            return iterators != other.iterators;
        }

        auto operator*() const
        {
            return transform_tuple([] (const auto& x) { return std::ref(*x); }, iterators);
        }

        IteratorTuple iterators;
    };

    //=========================================================================
    zipped_container_t(ContainerTuple&& containers) : containers(std::move(containers))
    {
    }

    auto begin() const
    {
        auto res = transform_tuple([] (const auto& x) { return std::begin(x); }, containers);
        return iterator<decltype(res)>{res};
    }

    auto end() const
    {
        auto res = transform_tuple([] (const auto& x) { return std::end(x); }, containers);
        return iterator<decltype(res)>{res};
    }

    template<typename Function>
    auto operator|(Function&& fn) const
    {
        return transformed_container_t<zipped_container_t, Function>(*this, fn);
    }

private:
    //=========================================================================
    template<typename Function, typename Tuple, std::size_t... Is>
    static auto transform_tuple_impl(Function&& fn, const Tuple& t, std::index_sequence<Is...>)
    {
        return std::make_tuple(fn(std::get<Is>(t))...);
    }

    template<typename Function, typename Tuple>
    static auto transform_tuple(Function&& fn, const Tuple& t)
    {
        return transform_tuple_impl(fn, t, std::make_index_sequence<std::tuple_size<Tuple>::value>());
    }

    ContainerTuple containers;
};




//=============================================================================
template<typename ContainerType, typename Function>
class nd::transformed_container_t
{
public:
    using value_type = std::invoke_result_t<Function, typename ContainerType::value_type>;

    //=========================================================================
    template<typename IteratorType>
    struct iterator
    {
        using iterator_category = std::input_iterator_tag;
        using value_type = value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator& operator++() { ++current; return *this; }
        bool operator==(const iterator& other) const { return current == other.current; }
        bool operator!=(const iterator& other) const { return current != other.current; }
        auto operator*() const { return function(*current); }

        IteratorType current;
        const Function& function;
    };

    //=========================================================================
    transformed_container_t(const ContainerType& container, const Function& function)
    : container(container)
    , function(function)
    {
    }
    auto begin() const { return iterator<decltype(container.begin())> {container.begin(), function}; }
    auto end() const { return iterator<decltype(container.end())> {container.end(), function}; }

private:
    //=========================================================================
    const ContainerType& container;
    const Function& function;
};




//=============================================================================
template<typename Range, typename Seed, typename Function>
auto nd::accumulate(Range&& rng, Seed&& seed, Function&& fn)
{
    return std::accumulate(rng.begin(), rng.end(), std::forward<Seed>(seed), std::forward<Function>(fn));
}

template<typename Range, typename Predicate>
auto nd::all_of(Range&& rng, Predicate&& pred)
{
    return std::all_of(rng.begin(), rng.end(), pred);
}

template<typename Range, typename Predicate>
auto nd::any_of(Range&& rng, Predicate&& pred)
{
    return std::any_of(rng.begin(), rng.end(), pred);
}

template<typename Range>
auto nd::distance(Range&& rng)
{
    return std::distance(rng.begin(), rng.end());
}

template<typename Range>
auto nd::enumerate(Range&& rng)
{
    return zip(range(distance(std::forward<Range>(rng))), std::forward<Range>(rng));
}

template<typename ValueType>
auto nd::range(ValueType count)
{
    return nd::range_container_t<ValueType>(0, count);
}

template<typename... ContainerTypes>
auto nd::zip(ContainerTypes&&... containers)
{
    using ValueType = std::tuple<typename std::remove_reference_t<ContainerTypes>::value_type...>;
    using ContainerTuple = std::tuple<ContainerTypes...>;
    return nd::zipped_container_t<ValueType, ContainerTuple>(std::forward_as_tuple(containers...));
}




//=============================================================================
template<std::size_t Rank, typename ValueType, typename DerivedType>
class nd::short_sequence_t
{
public:

    using value_type = ValueType;

    //=========================================================================
    static DerivedType uniform(ValueType arg)
    {
        DerivedType result;

        for (auto n : range(Rank))
        {
            result.memory[n] = arg;
        }
        return result;
    }

    template<typename Range>
    static DerivedType from_range(Range&& rng)
    {
        if (distance(rng) != Rank)
        {
            throw std::logic_error("sequence constructed from range of wrong size");
        }

        DerivedType result;
        for (const auto& [n, a] : enumerate(rng))
        {
            result.memory[n] = a;
        }
        return result;
    }

    short_sequence_t()
    {
        for (auto n : range(Rank))
        {
            memory[n] = ValueType();
        }
    }

    short_sequence_t(std::initializer_list<ValueType> args)
    {
        for (const auto& [n, a] : enumerate(args))
        {
            memory[n] = a;
        }
    }

    bool operator==(const DerivedType& other) const
    {
        return all_of(zip(*this, other), [] (const auto& t) { return std::get<0>(t) == std::get<1>(t); });
    }

    bool operator!=(const DerivedType& other) const
    {
        return any_of(zip(*this, other), [] (const auto& t) { return std::get<0>(t) != std::get<1>(t); });
    }

    std::size_t size() const { return accumulate(*this, 1, std::multiplies<>()); }
    const ValueType* data() const { return memory; }
    const ValueType* begin() const { return memory; }
    const ValueType* end() const { return memory + Rank; }
    const ValueType& operator[](std::size_t n) const { return memory[n]; }
    ValueType* data() { return memory; }
    ValueType* begin() { return memory; }
    ValueType* end() { return memory + Rank; }
    ValueType& operator[](std::size_t n) { return memory[n]; }

private:
    //=========================================================================
    ValueType memory[Rank];
};




//=============================================================================
template<std::size_t Rank>
class nd::shape_t : public nd::short_sequence_t<Rank, std::size_t, shape_t<Rank>>
{
public:
    using short_sequence_t<Rank, std::size_t, shape_t<Rank>>::short_sequence_t;

    bool contains(const index_t<Rank>& index) const
    {
        return all_of(zip(index, *this), [] (const auto& t) { return std::get<0>(t) < std::get<1>(t); });
    }

    template<typename... Args>
    bool contains(Args... args) const
    {
        return contains(make_index(args...));
    }
};




//=============================================================================
template<std::size_t Rank>
class nd::index_t : public nd::short_sequence_t<Rank, std::size_t, index_t<Rank>>
{
public:
    using short_sequence_t<Rank, std::size_t, index_t<Rank>>::short_sequence_t;
};




//=============================================================================
template<std::size_t Rank>
class nd::jumps_t : public nd::short_sequence_t<Rank, long, jumps_t<Rank>>
{
public:
    using short_sequence_t<Rank, long, jumps_t<Rank>>::short_sequence_t;
};




//=============================================================================
template<std::size_t Rank>
class nd::memory_strides_t : public nd::short_sequence_t<Rank, std::size_t, memory_strides_t<Rank>>
{
public:
    using short_sequence_t<Rank, std::size_t, memory_strides_t<Rank>>::short_sequence_t;

    std::size_t compute_offset(const index_t<Rank>& index) const
    {
        auto mul_tuple = [] (auto t) { return std::get<0>(t) * std::get<1>(t); };
        return accumulate(zip(index, *this) | mul_tuple, 0, std::plus<>());
    }

    template<typename... Args>
    std::size_t compute_offset(Args... args) const
    {
        return compute_offset(make_index(args...));
    }
};




//=============================================================================
template<std::size_t Rank>
class nd::access_pattern_t
{
public:

    using value_type = index_t<Rank>;
    static constexpr std::size_t rank = Rank;

    //=========================================================================
    struct iterator
    {
        using iterator_category = std::input_iterator_tag;
        using value_type = index_t<Rank>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator& operator++() { accessor.advance(current); return *this; }
        bool operator==(const iterator& other) const { return current == other.current; }
        bool operator!=(const iterator& other) const { return current != other.current; }
        const index_t<Rank>& operator*() const { return current; }

        access_pattern_t accessor;
        index_t<Rank> current;
    };

    //=========================================================================
    template<typename... Args> access_pattern_t with_start(Args... args) const { return { make_index(args...), final, jumps }; }
    template<typename... Args> access_pattern_t with_final(Args... args) const { return { start, make_index(args...), jumps }; }
    template<typename... Args> access_pattern_t with_jumps(Args... args) const { return { start, final, make_jumps(args...) }; }

    access_pattern_t with_start(index_t<Rank> arg) const { return { arg, final, jumps }; }
    access_pattern_t with_final(index_t<Rank> arg) const { return { start, arg, jumps }; }
    access_pattern_t with_jumps(jumps_t<Rank> arg) const { return { start, final, arg }; }

    std::size_t size() const
    {
        return accumulate(shape(), 1, std::multiplies<int>());
    }

    bool empty() const
    {
        return any_of(shape(), [] (auto s) { return s == 0; });
    }

    auto shape() const
    {
        auto s = shape_t<Rank>();

        for (std::size_t n = 0; n < Rank; ++n)
        {
            s[n] = final[n] / jumps[n] - start[n] / jumps[n];
        }
        return s;
    }

    bool operator==(const access_pattern_t& other) const
    {
        return
        start == other.start &&
        final == other.final &&
        jumps == other.jumps;
    }

    bool operator!=(const access_pattern_t& other) const
    {
        return
        start != other.start ||
        final != other.final ||
        jumps != other.jumps;
    }

    bool advance(index_t<Rank>& index) const
    {
        int n = Rank - 1;

        index[n] += jumps[n];

        while (index[n] >= final[n])
        {
            if (n == 0)
            {
                index = final;
                return false;
            }
            index[n] = start[n];

            --n;

            index[n] += jumps[n];
        }
        return true;
    }

    index_t<Rank> map_index(const index_t<Rank>& index) const
    {
        index_t<Rank> result;

        for (std::size_t n = 0; n < Rank; ++n)
        {
            result[n] = start[n] + jumps[n] * index[n];
        }
        return result;
    }

    bool contains(const index_t<Rank>& index) const
    {
        return shape().contains(index);
    }

    template<typename... Args>
    bool contains(Args... args) const
    {
        return contains(make_index(args...));
    }

    iterator begin() const { return { *this, start }; }
    iterator end() const { return { *this, final }; }

    //=========================================================================
    index_t<Rank> start = make_uniform_index<Rank>(0);
    index_t<Rank> final = make_uniform_index<Rank>(0);
    jumps_t<Rank> jumps = make_uniform_jumps<Rank>(1);
};




//=============================================================================
template<std::size_t Rank, typename Provider>
class nd::array_t
{
public:

    using Accessor = access_pattern_t<Rank>;
    using provider_type = Provider;
    using accessor_type = Accessor;
    using value_type = typename Provider::value_type;
    static constexpr std::size_t rank = Rank;

    //=========================================================================
    array_t(Provider&& provider, Accessor&& accessor)
    : provider(std::move(provider))
    , accessor(std::move(accessor))
    {
    }

    template<typename... Args> decltype(auto) operator()(Args... args) const { return provider(accessor.map_index(make_index(args...))); }
    template<typename... Args> decltype(auto) operator()(Args... args)       { return provider(accessor.map_index(make_index(args...))); }

    decltype(auto) operator()(const index_t<Rank>& index) const { return provider(accessor.map_index(index)); }
    decltype(auto) operator()(const index_t<Rank>& index)       { return provider(accessor.map_index(index)); }

    auto shape() const { return provider.shape(); }
    auto size() const { return provider.size(); }

    const Provider& get_provider() const
    {
        return provider;
    }

    auto unique() const
    {
        return make_array(evaluate_as_unique(provider, accessor));
    }

    auto shared() const
    {
        return make_array(evaluate_as_unique(provider, accessor).shared());
    }

private:
    //=========================================================================
    Provider provider;
    Accessor accessor;
};




//=============================================================================
template<std::size_t Rank>
class nd::index_provider_t
{
public:

    using value_type = index_t<Rank>;
    static constexpr std::size_t rank = Rank;

    //=========================================================================
    index_provider_t(shape_t<Rank> the_shape) : the_shape(the_shape) {}
    auto operator()(const index_t<Rank>& index) const { return index; }
    auto shape() const { return the_shape; }
    auto size() { return the_shape.size(); }

private:
    //=========================================================================
    shape_t<Rank> the_shape;
};




//=============================================================================
template<std::size_t Rank, typename ValueType>
class nd::shared_provider_t
{
public:

    using value_type = ValueType;
    static constexpr std::size_t rank = Rank;

    //=========================================================================
    shared_provider_t(nd::shape_t<Rank> the_shape, std::shared_ptr<nd::buffer_t<ValueType>> buffer)
    : the_shape(the_shape)
    , strides(make_strides_row_major(the_shape))
    , buffer(buffer)
    {
        if (the_shape.size() != buffer->size())
        {
            throw std::logic_error("shape and buffer sizes do not match");
        }
    }

    const ValueType& operator()(const index_t<Rank>& index) const
    {
        return buffer->operator[](strides.compute_offset(index));
    }

    auto shape() const { return the_shape; }
    auto size() const { return the_shape.size(); }
    const ValueType* data() const { return buffer->data(); }

private:
    //=========================================================================
    shape_t<Rank> the_shape;
    memory_strides_t<Rank> strides;
    std::shared_ptr<buffer_t<ValueType>> buffer;
};




//=============================================================================
template<std::size_t Rank, typename ValueType>
class nd::unique_provider_t
{
public:

    using value_type = ValueType;
    static constexpr std::size_t rank = Rank;

    //=========================================================================
    unique_provider_t(nd::shape_t<Rank> the_shape, nd::buffer_t<ValueType>&& buffer)
    : the_shape(the_shape)
    , strides(make_strides_row_major(the_shape))
    , buffer(std::move(buffer))
    {
        if (the_shape.size() != unique_provider_t::buffer.size())
        {
            throw std::logic_error("shape and buffer sizes do not match");
        }
    }

    auto shared() const &
    {
        return shared_provider_t(the_shape, std::make_shared<buffer_t<ValueType>>(buffer));
    }

    auto shared() &&
    {
        return shared_provider_t(the_shape, std::make_shared<buffer_t<ValueType>>(std::move(buffer)));
    }

    const ValueType& operator()(const index_t<Rank>& index) const
    {
        return buffer.operator[](strides.compute_offset(index));
    }

    template<typename... Args>
    const ValueType& operator()(Args... args) const
    {
        return operator()(make_index(args...));
    }

    ValueType& operator()(const index_t<Rank>& index)
    {
        return buffer.operator[](strides.compute_offset(index));
    }

    template<typename... Args>
    ValueType& operator()(Args... args)
    {
        return operator()(make_index(args...));
    }

    auto shape() const { return the_shape; }
    auto size() const { return the_shape.size(); }
    const ValueType* data() const { return buffer.data(); }

private:
    //=========================================================================
    shape_t<Rank> the_shape;
    memory_strides_t<Rank> strides;
    buffer_t<ValueType> buffer;
};




//=============================================================================
template<std::size_t Rank, typename ValueType, typename ArrayTuple>
class nd::zipped_provider_t
{
public:

    using value_type = ValueType;
    static constexpr std::size_t rank = Rank;

    //=========================================================================
    zipped_provider_t(shape_t<Rank> the_shape, ArrayTuple&& arrays)
    : the_shape(the_shape)
    , arrays(std::move(arrays))
    {
    }
    auto operator()(const index_t<Rank>& index) const { return transform_tuple([index] (auto&& A) { return A(index); }, arrays); }
    auto shape() const { return the_shape; }
    auto size() { return the_shape.size(); }

private:
    //=========================================================================
    template<typename Function, typename Tuple, std::size_t... Is>
    static auto transform_tuple_impl(Function&& fn, const Tuple& t, std::index_sequence<Is...>)
    {
        return std::make_tuple(fn(std::get<Is>(t))...);
    }

    template<typename Function, typename Tuple>
    static auto transform_tuple(Function&& fn, const Tuple& t)
    {
        return transform_tuple_impl(fn, t, std::make_index_sequence<std::tuple_size<Tuple>::value>());
    }

    shape_t<Rank> the_shape;
    ArrayTuple arrays;
};




//=============================================================================
template<typename ValueType>
class nd::buffer_t
{
public:

    using value_type = ValueType;

    //=========================================================================
    buffer_t() {}

    buffer_t(const buffer_t& other)
    {
        memory = new ValueType[other.count];
        count = other.count;

        for (std::size_t n = 0; n < count; ++n)
        {
            memory[n] = other.memory[n];
        }
    }

    buffer_t(buffer_t&& other)
    {
        memory = other.memory;
        count = other.count;
        other.memory = nullptr;
        other.count = 0;
    }

    ~buffer_t() { delete [] memory; }

    explicit buffer_t(std::size_t count, const ValueType& value = ValueType())
    : count(count)
    , memory(new ValueType[count])
    {
        for (std::size_t n = 0; n < count; ++n)
        {
            memory[n] = value;
        }
    }

    template<class IteratorType>
    buffer_t(IteratorType first, IteratorType last)
    : count(std::distance(first, last))
    , memory(new ValueType[count])
    {
        for (std::size_t n = 0; n < count; ++n)
        {
            memory[n] = *first++;
        }
    }

    buffer_t& operator=(const buffer_t& other)
    {
        delete [] memory;
        count = other.count;
        memory = new ValueType[count];

        for (std::size_t n = 0; n < count; ++n)
        {
            memory[n] = other.memory[n];
        }
        return *this;
    }

    buffer_t& operator=(buffer_t&& other)
    {
        delete [] memory;
        memory = other.memory;
        count = other.count;

        other.memory = nullptr;
        other.count = 0;
        return *this;
    }

    bool operator==(const buffer_t& other) const
    {
        return count == other.count
        && all_of(zip(*this, other), [] (const auto& t) { return std::get<0>(t) == std::get<1>(t); });
    }

    bool operator!=(const buffer_t& other) const
    {
        return count != other.count
        || any_of(zip(*this, other), [] (const auto& t) { return std::get<0>(t) != std::get<1>(t); });
    }

    bool empty() const { return count == 0; }
    std::size_t size() const { return count; }

    const ValueType* data() const { return memory; }
    const ValueType* begin() const { return memory; }
    const ValueType* end() const { return memory + count; }
    const ValueType& operator[](std::size_t offset) const { return memory[offset]; }
    const ValueType& at(std::size_t offset) const
    {
        if (offset >= count)
        {
            throw std::out_of_range("buffer_t index out of range on index "
                + std::to_string(offset) + " / "
                + std::to_string(count));
        }
        return memory[offset];
    }

    ValueType* data() { return memory; }
    ValueType* begin() { return memory; }
    ValueType* end() { return memory + count; }
    ValueType& operator[](std::size_t offset) { return memory[offset]; }
    ValueType& at(std::size_t offset)
    {
        if (offset >= count)
        {
            throw std::out_of_range("buffer_t index out of range on index "
                + std::to_string(offset) + " / "
                + std::to_string(count));
        }
        return memory[offset];
    }

private:
    //=========================================================================
    std::size_t count = 0;
    ValueType* memory = nullptr;
};




//=============================================================================
template<typename... Args>
auto nd::make_shape(Args... args)
{
    return shape_t<sizeof...(Args)>({std::size_t(args)...});
}

template<typename... Args>
auto nd::make_index(Args... args)
{
    return index_t<sizeof...(Args)>({std::size_t(args)...});
}

template<typename... Args>
auto nd::make_jumps(Args... args)
{
    return jumps_t<sizeof...(Args)>({long(args)...});
}

template<std::size_t Rank, typename Arg>
auto nd::make_uniform_shape(Arg arg)
{
    return shape_t<Rank>::uniform(arg);
}

template<std::size_t Rank, typename Arg>
auto nd::make_uniform_index(Arg arg)
{
    return index_t<Rank>::uniform(arg);
}

template<std::size_t Rank, typename Arg>
auto nd::make_uniform_jumps(Arg arg)
{
    return jumps_t<Rank>::uniform(arg);
}

template<std::size_t Rank>
auto nd::make_strides_row_major(shape_t<Rank> shape)
{
    memory_strides_t<Rank> result;
    result[Rank - 1] = 1;

    if constexpr (Rank > 1)
    {
        for (int n = Rank - 2; n >= 0; --n)
        {
            result[n] = result[n + 1] * shape[n + 1];
        }
    }
    return result;
}

template<std::size_t Rank>
auto nd::make_access_pattern(shape_t<Rank> shape)
{
    return access_pattern_t<Rank>().with_final(index_t<Rank>::from_range(shape));
}

template<typename... Args>
auto nd::make_access_pattern(Args... args)
{
    return access_pattern_t<sizeof...(Args)>().with_final(args...);
}

template<typename... Args>
auto nd::make_index_provider(Args... args)
{
    return index_provider_t<sizeof...(Args)>(make_shape(args...));
}

template<typename ValueType, std::size_t Rank>
auto nd::make_shared_provider(shape_t<Rank> shape)
{
    auto buffer = std::make_shared<buffer_t<ValueType>>(shape.size());
    return shared_provider_t<Rank, ValueType>(shape, buffer);
}

template<typename ValueType, typename... Args>
auto nd::make_shared_provider(Args... args)
{
    return make_shared_provider<ValueType>(make_shape(args...));
}

template<typename ValueType, std::size_t Rank>
auto nd::make_unique_provider(shape_t<Rank> shape)
{
    auto buffer = buffer_t<ValueType>(shape.size());
    return unique_provider_t<Rank, ValueType>(shape, std::move(buffer));
}

template<typename ValueType, typename... Args>
auto nd::make_unique_provider(Args... args)
{
    return make_unique_provider<ValueType>(make_shape(args...));
}

template<typename Provider, typename Accessor>
auto nd::make_array(Provider&& provider, Accessor&& accessor)
{
    return array_t<Provider::rank, Provider>(std::forward<Provider>(provider), std::forward<Accessor>(accessor));
}

template<typename Provider>
auto nd::make_array(Provider&& provider)
{
    return array_t<Provider::rank, Provider>(std::forward<Provider>(provider), make_access_pattern(provider.shape()));
}

template<typename Provider, typename Accessor>
auto nd::evaluate_as_unique(Provider&& source_provider, Accessor&& source_accessor)
{
    using value_type = typename std::remove_reference_t<Provider>::value_type;
    auto target_shape = source_accessor.shape();
    auto target_accessor = make_access_pattern(target_shape);
    auto target_provider = make_unique_provider<value_type>(target_shape);

    for (auto&& [target_index, source_index] : zip(target_accessor, source_accessor))
    {
        target_provider(target_index) = source_provider(source_index);
    }
    return target_provider;
}

template<typename Provider, typename Accessor>
auto nd::evaluate_as_shared(Provider&& source_provider, Accessor&& source_accessor)
{
    return evaluate_as_unique(
        std::forward<Provider>(source_provider),
        std::forward<Accessor>(source_accessor)).shared();
}

template<typename Provider>
auto nd::evaluate_as_unique(Provider&& provider)
{
    return evaluate_as_unique(std::forward<Provider>(provider), make_access_pattern(provider.shape()));
}

template<typename Provider>
auto nd::evaluate_as_shared(Provider&& provider)
{
    return evaluate_as_unique(std::forward<Provider>(provider)).shared();
}

template<typename... ArrayTypes>
auto nd::zip_arrays(ArrayTypes&&... arrays)
{
    constexpr auto Rank = std::remove_reference_t<decltype(std::get<0>(std::forward_as_tuple(arrays...)))>::rank;
    using ValueType = std::tuple<typename std::remove_reference_t<ArrayTypes>::value_type...>;
    using ArrayTuple = std::tuple<ArrayTypes...>;
    auto shape = std::get<0>(std::forward_as_tuple(arrays...)).shape();
    return zipped_provider_t<Rank, ValueType, ArrayTuple>(shape, std::forward_as_tuple(arrays...));
}

