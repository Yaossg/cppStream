#ifndef __CPP_STREAM_HPP__
#define __CPP_STREAM_HPP__
#include <functional>
#include <stdexcept>
#include <optional>
#include <vector>
#include <set>
#ifndef CPP_STREAM_NO_TYPEINFO
//type-erasure
#include <typeinfo>
#endif

#define Buildable template<typename Builder> \
	decltype(auto) operator>>(Builder builder) const& \
	{ return builder.build(*this); } \
	template<typename Builder> \
	decltype(auto) operator>>(Builder builder) && \
	{ return builder.build(std::move(*this)); }

namespace yaossg::stream { 

#ifndef CPP_STREAM_NO_EXCEPTION
struct stream_exception : std::logic_error {
	stream_exception() : logic_error("stream_exception") {}
protected:
	stream_exception(char const* str) : logic_error(str) {}
};

struct endless_stream_exception : stream_exception {
	endless_stream_exception() : stream_exception("endless_stream_exception") {}
};

#endif
template<typename Stream>
void throw_if_endless(Stream const& stream) {
	if(stream.endless())
#ifndef CPP_STREAM_NO_EXCEPTION
		throw endless_stream_exception();
#else
		std::abort();
#endif
}
template<typename Pred>
struct MakeBuilder { 
	Pred pred;		
	
	explicit MakeBuilder(Pred pred) : pred(pred) {}
	
	template<typename Stream>
	auto build(Stream const& stream) {
		return pred(stream);
	}
};

template<typename Pred>
auto make_builder(Pred pred) {
	return MakeBuilder(pred);
}

template<template<typename...>typename Stream, typename... Args>
auto builder_of(Args... args) {
	return make_builder([args...](auto stream){ return Stream<decltype(stream), Args...>(std::move(stream), args...);});
}

template<typename T>
struct remove_optional {
	using type = T;
};

template<typename T>
struct remove_optional<std::optional<T>> {
	using type = T;
};

template<typename T>
using remove_optional_t = typename remove_optional<T>::type;

template<typename T>
using valueType = typename T::value_type;



template<typename T>
struct EndlessEmptyStream {
	using value_type = T;
	
	value_type front() {} // UB if invokes it
	
	bool next() {
		while(true); // UB if invokes it
		return false;
	}
	
	bool endless() const {
		return true;
	}
	
	Buildable;
};

template<typename T>
struct EmptyStream {
	using value_type = T;
	
	value_type front() {} // UB if invokes it
	
	bool next() {
		return false;
	}
	
	bool endless() const {
		return false;
	}
	
	Buildable;
};

template<typename First>
struct EndlessIteratorStream {
	using value_type = valueType<std::iterator_traits<First>>;
	
	First current, upcoming;
	
	EndlessIteratorStream(First first)
		: current(first), upcoming(first) {}
	
	decltype(auto) front() {
		return *current;
	}
	
	bool next() {
		return current = upcoming++, true;
	}
	
	bool endless() const {
		return true;
	}
	
	Buildable;
};

template<typename First, typename Last>
struct IteratorStream {
	using value_type = valueType<std::iterator_traits<First>>;
	
	First current, upcoming;
	Last last;
	bool unchecked;
	
	IteratorStream(First first, Last last, bool unchecked)
		: current(first), upcoming(first), last(last), unchecked(unchecked) {}
	
	decltype(auto) front() {
		return *current;
	}
	
	bool next() {
		return upcoming == last ? false : (current = upcoming++, true);
	}
	
	bool endless() const {
		return unchecked;
	}
	
	Buildable;
};

template<typename IntType>
struct IntegerStream {
	using value_type = IntType;
	
	IntType current, step;
	
	IntegerStream(IntType first, IntType step = 1) 
		: current(first - step), step(step) {}
	
	decltype(auto) front() {
		return current;
	}
	
	bool next() {
		return current += step, true;
	}
	
	bool endless() const {
		return true;
	}
	
	Buildable;
};

template<typename Getter>
struct GenerateStream {
	using value_type = std::invoke_result_t<Getter>;
	
	Getter getter;
	std::optional<value_type> cache;
	
	GenerateStream(Getter getter) 
		: getter(getter) {}
	
	decltype(auto) front() {
		return *cache;
	}
	
	bool next() {
		return cache = std::invoke(getter), true;
	}
	
	bool endless() const {
		return true;
	}
	
	Buildable;
};

template<typename Stream, typename Pred>
struct FilterStream {
	using value_type = valueType<Stream>;
	
	Stream stream;
	Pred pred;
	
	FilterStream(Stream stream, Pred pred)
		: stream(stream), pred(pred) {}
	
	decltype(auto) front() {
		return stream.front();
	}
	
	bool next() {
		while(stream.next())
			if(std::invoke(pred, stream.front()))
				return true;
		return false;
	}
	
	bool endless() const {
		return stream.endless();
	}
	
	Buildable;
};

template<typename Stream, typename Pred>
struct MapStream {
	using value_type = remove_optional_t<std::remove_cv_t<std::invoke_result_t<Pred, valueType<Stream>>>>;
	
	Stream stream;
	Pred pred;
	std::optional<value_type> cache_value;

	MapStream(Stream stream, Pred pred)
		: stream(stream), pred(pred) {}

	decltype(auto) front() {
		return *cache_value;
	}

	bool next() {
		while(stream.next())
			if(cache_value = std::invoke(pred, stream.front()))
				return true;
		return false;
	}
	
	bool endless() const {
		return stream.endless();
	}
	
	Buildable;
};

template<typename Stream>
struct TakeStream {
	using value_type = valueType<Stream>;
	
	Stream stream;
	size_t count;
	
	TakeStream(Stream stream, size_t count)
		: stream(stream), count(count + 1){}
		
	decltype(auto) front() {
		return stream.front();
	}
	
	bool next() {
		return count && --count != 0 && stream.next();
	}
	
	bool endless() const {
		return false;
	}
	
	Buildable;
};

template<typename Stream>
struct SkipStream {
	using value_type = valueType<Stream>;
	
	Stream stream;
	size_t count;
	
	SkipStream(Stream stream, size_t count)
		: stream(stream), count(count + 1) {}
		
	decltype(auto) front() {
		return stream.front();
	}
	
	bool next() {
		while(count && --count != 0 && stream.next());
		return stream.next();
	}
	
	bool endless() const {
		return stream.endless();
	}
	
	Buildable;
};

template<typename Stream, typename Pred>
struct TakeWhileStream {
	using value_type = valueType<Stream>;
	
	Stream stream;
	Pred pred;
	bool left;
	
	TakeWhileStream(Stream stream, Pred pred)
		: stream(stream), pred(pred), left(true) {}
		
	decltype(auto) front() {
		return stream.front();
	}
	
	bool next() {
		return left && (left = (stream.next() && std::invoke(pred, stream.front())));
	}
	
	bool endless() const {
		return stream.endless(); // that depends on pred
	}
	
	Buildable;
};

template<typename Stream, typename Pred>
struct SkipWhileStream {
	using value_type = valueType<Stream>;
	
	Stream stream;
	Pred pred;
	bool left;
	
	SkipWhileStream(Stream stream, Pred pred)
		: stream(stream), pred(pred), left(true){}
		
	decltype(auto) front() {
		return stream.front();
	}
	
	bool next() {
		while(left) {
			if(!stream.next()) 
				return left = false;
			if(!std::invoke(pred, stream.front()))
				return !(left = false);
		}
		return stream.next();
	}
	
	bool endless() const {
		return stream.endless();
	}
	
	Buildable;
};

template<typename Stream, typename Compare>
struct SortStream {
	using value_type = valueType<Stream>;
	
	Stream stream;
	Compare compare;
	std::vector<std::remove_cv_t<std::remove_reference_t<value_type>>> sorted;
	size_t current;
	
	SortStream(Stream stream, Compare compare) 
		: stream(stream), compare(compare), current(0) {}
	
	decltype(auto) front() {
		return sorted[current - 1];
	}
	
	bool next() {
		if(!current++) {
			while(stream.next())
				sorted.push_back(stream.front());
			std::stable_sort(sorted.begin(), sorted.end(), compare);
		}
		return current - 1 < sorted.size();
	}
	
	bool endless() const {
		return stream.endless();
	}
	
	Buildable;
};

template<typename Stream>
struct ReverseStream {
	using value_type = valueType<Stream>;
	
	Stream stream;
	std::vector<std::remove_cv_t<std::remove_reference_t<value_type>>> reversed;
	size_t current;
	
	ReverseStream(Stream stream) 
		: stream(stream), current(0) {}
	
	decltype(auto) front() {
		return reversed[current - 1];
	}
	
	bool next() {
		if(!current++) {
			while(stream.next())
				reversed.push_back(stream.front());
			std::reverse(reversed.begin(), reversed.end());
		}
		return current - 1 < reversed.size();
	}
	
	bool endless() const {
		return stream.endless();
	}
	
	Buildable;
};

template<typename Stream, typename Set>
struct DistinctStream {
	using value_type = valueType<Stream>;
	
	Stream stream;
	Set set;
	typename Set::const_iterator current;
	
	DistinctStream(Stream stream, Set set) 
		: stream(stream), set(set), current() {}
	
	decltype(auto) front() {
		return *current;
	}
	
	bool next() {
		while(stream.next())
			if(auto [next, success] = set.insert(stream.front()); success)
				return current = next, true;
		return false;
	}
	
	bool endless() const {
		return stream.endless();
	}
	
	Buildable;
};

template<typename Stream, typename Peeker>
struct PeekStream {
	using value_type = valueType<Stream>;
	
	Stream stream;
	Peeker peeker;
	
	PeekStream(Stream stream, Peeker peeker) 
		: stream(stream), peeker(peeker) {}
	
	decltype(auto) front() {
		return stream.front();
	}
	
	bool next() {
		return stream.next() ? std::invoke(peeker, stream.front()), true : false;
	}
	
	bool endless() const {
		return stream.endless();
	}
	
	Buildable;
};

template<typename Stream>
struct MakeEndlessStream {
	using value_type = valueType<Stream>;
	
	Stream stream;
	
	MakeEndlessStream(Stream stream) 
		: stream(stream) {}
	
	decltype(auto) front() {
		return stream.front();
	}
	
	bool next() {
		return stream.next();
	}
	
	bool endless() const {
		return true;
	}
	
	Buildable;
};

template<typename Stream>
struct TailRepeatStream {
	using value_type = remove_optional_t<valueType<Stream>>;
	
	Stream stream;
	bool left;
	std::optional<value_type> cache;
	
	TailRepeatStream(Stream stream) 
		: stream(stream), cache(), left(true) {}
	
	decltype(auto) front() {
		return left ? (cache = stream.front()).value() : cache.value();
	}
	
	bool next() {
		if(!stream.next()) 
			left = false;
		return true;
	}
	
	bool endless() const {
		return true;
	}
	
	Buildable;
};

template<typename Stream>
struct LoopStream {
	using value_type = valueType<Stream>;
	
	Stream stream, cache;
	
	LoopStream(Stream stream) 
		: stream(stream), cache(stream) {}
	
	decltype(auto) front() {
		return stream.front();
	}
	
	bool next() {
		while(!stream.next())
			stream = cache;
		return true;
	}
	
	bool endless() const {
		return true;
	}
	
	Buildable;
};

template<typename Stream>
struct EndlessFlatStream {
	using value_type = valueType<valueType<Stream>>;
	
	Stream stream;
	bool left;
	
	EndlessFlatStream(Stream stream) 
		: stream(stream), left(false) {}
	
	decltype(auto) front() {
		return stream.front().front();
	}
	
	bool next() {
		return (left && stream.front().next()) || ((left = (left = false, stream.next())) && stream.front().next());
	}
	
	bool endless() const {
		return true; // that depends on what the stream contains 
	}
	
	Buildable;
};

template<typename Stream>
struct FlatStream {
	using value_type = valueType<valueType<Stream>>;
	
	Stream stream;
	std::vector<valueType<Stream>> streams;
	size_t current; 
	
	FlatStream(Stream stream) 
		: stream(stream), streams(), current(0) {
		while(stream.next())
			streams.push_back(stream.front());
	}
	
	decltype(auto) front() {
		return streams[current].front();
	}
	
	bool next() {
		return current != streams.size() && (streams[current].next() || (++current && next()));
	}
	
	bool endless() const {
		return std::all_of(streams.begin(), streams.end(), [](auto const& stream) { return stream.endless(); });	
	}
	
	Buildable;
};

template<typename StreamA, typename StreamB>
struct JoinStreamAB {
	static_assert(std::is_convertible_v<valueType<StreamB>, valueType<StreamA>>);
	using value_type = valueType<StreamA>; 
	
	StreamA streamA;
	StreamB streamB;
	bool B;
	
	JoinStreamAB(StreamA&& streamA, StreamB&& streamB) 
		: streamA(streamA), streamB(streamB), B(false) {}
	
	decltype(auto) front() {
		return B ? streamB.front() : streamA.front();
	}
	
	bool next() {
		return streamA.next() || (B = true, streamB.next());
	}
	
	bool endless() const {
		return streamA.endless() || streamB.endless();
	}
	
	Buildable;
};

template<typename ...Streams>
struct JoinStreams {};

template<typename StreamA, typename ...Streams>
struct JoinStreams<StreamA, Streams...> : JoinStreamAB<StreamA, JoinStreams<Streams...>> {
	using extended = JoinStreamAB<StreamA, JoinStreams<Streams...>>;
	JoinStreams(StreamA&& streamA, Streams&&... streams)
		: extended(std::forward<StreamA&&>(streamA), JoinStreams<Streams...>(std::move(streams)...)) {}
	using extended::front;
	using extended::next;
	using extended::endless;
	
	Buildable;
};

template<typename StreamA, typename StreamB>
struct JoinStreams<StreamA, StreamB> : JoinStreamAB<StreamA, StreamB> {
	using extended = JoinStreamAB<StreamA, StreamB>;
	using extended::extended;
	using extended::front;
	using extended::next;
	using extended::endless;
	
	Buildable;
};

template<typename StreamA>
struct JoinStreams<StreamA> {};

template<typename Pred, typename ...Streams>
struct CombineStreams {
	using value_type = remove_optional_t<std::invoke_result_t<Pred, valueType<Streams>...>>;
	
	Pred pred;
	std::optional<value_type> cache;
	std::tuple<Streams...> streams;
	
	CombineStreams(Pred pred, Streams&&... streams) 
		: pred(pred), streams(streams...) {}
	
	decltype(auto) front() {
		return *cache;
	}
	
	bool next() {
		return std::apply([](auto&... streams){ return (streams.next() && ...); }, streams) 
		&& ((cache = std::apply([this](auto&... streams){ return std::invoke(pred, streams.front()...); }, streams)), true);
	}
	
	bool endless() const {
		return std::apply([](auto&... streams){ return (streams.endless() && ...); }, streams);
	}
	
	Buildable;
};

struct IterableBuilder {
	template<typename Stream>
	struct Iterable {
		struct Iterator {
			using iterator_category = std::forward_iterator_tag;
			using value_type = std::remove_cv_t<std::remove_reference_t<valueType<Stream>>>;
			using difference_type = size_t;
			using pointer = value_type*;
			using reference = value_type&;
			
			Stream* stream;
			bool next;
			
			Iterator() : stream(nullptr), next(false) {}
			
			explicit Iterator(Stream* stream) 
				: stream(stream), next(stream->next()) {}
			
			Iterator& operator++() {
				next = stream->next();
				return *this;
			}
			
			auto operator++(int) {
				struct TempIterator {
					valueType<Stream> cache_value;
					TempIterator(Iterator* iter) 
						: cache_value(iter->stream->front()) {}
					
					decltype(auto) operator*() const {return cache_value;}
					
				} temp(this);
				++*this;
				return temp;
			}
			friend bool operator==(Iterator left, Iterator right) {
				return !left.next && right.stream == nullptr;
			}
			friend bool operator!=(Iterator left, Iterator right) {
				return !(left == right);
			}
			decltype(auto) operator*() const {return stream->front();}
		};
		Stream stream;
		Iterable(Stream stream)
			: stream(stream) {}
		Iterator begin() { return Iterator(&stream); }
		Iterator end() { return Iterator(); }
		
	};
	template<typename Stream>
	auto build(Stream stream) {
		return Iterable(std::move(stream));
	}
	
};

template<typename Pred>
struct ForEachBuilder {
	Pred pred;
	
	explicit ForEachBuilder(Pred pred) : pred(pred) {}

	template<typename Stream>
	auto build(Stream stream) {
		throw_if_endless(stream);
		while(stream.next())
			std::invoke(pred, stream.front());
	}
};

template<typename BiPred>
struct ReduceBuilder {
	BiPred biPred;
	
	explicit ReduceBuilder(BiPred biPred) : biPred(biPred) {}
	
	template<typename Stream>
	auto build(Stream stream) {
		throw_if_endless(stream);
		std::optional<valueType<Stream>> init = stream.next() ? std::optional(stream.front()) : std::nullopt;
		while(stream.next())
			init = std::invoke(biPred, init.value(), stream.front());
		return init;
	}
};

template<typename Compare>
struct MinBuilder {
	Compare compare;
	
	explicit MinBuilder(Compare compare) : compare(compare) {}
	
	template<typename Stream>
	auto build(Stream stream) {
		throw_if_endless(stream);
		std::optional<valueType<Stream>> init = stream.next() ? std::optional(stream.front()) : std::nullopt;
		while(stream.next())
			init = std::min(stream.front(), init.value(), compare);
		return init;
	}
};

template<typename Compare>
struct MaxBuilder {
	Compare compare;
	
	explicit MaxBuilder(Compare compare) : compare(compare) {}
	
	template<typename Stream>
	auto build(Stream stream) {
		throw_if_endless(stream);
		std::optional<valueType<Stream>> init = stream.next() ? std::optional(stream.front()) : std::nullopt;
		while(stream.next())
			init = std::max(stream.front(), init.value(), compare);
		return init;
	}
};

template<typename Compare>
struct MinMaxBuilder {
	Compare compare;
	
	explicit MinMaxBuilder(Compare compare) : compare(compare) {}
	
	template<typename Stream>
	auto build(Stream stream) const
		-> std::optional<std::pair<valueType<Stream>, valueType<Stream>>> {
		throw_if_endless(stream);
		if(stream.next()) {
			valueType<Stream> min = stream.front(), max = stream.front();
			while(stream.next()) {
				min = std::min(stream.front(), min, compare);
				max = std::max(stream.front(), max, compare);
			}
			return std::pair(min, max);
		}
		return {};
	}
};

template<typename Pred>
struct AllMatchBuilder {
	Pred pred;
	
	explicit AllMatchBuilder(Pred pred) : pred(pred) {}
	
	template<typename Stream>
	auto build(Stream stream) {
		throw_if_endless(stream);
		while(stream.next())
			if(!std::invoke(pred, stream.front()))
				return false;
		return true;
	}
};

template<typename Pred>
struct AnyMatchBuilder {
	Pred pred;
	
	explicit AnyMatchBuilder(Pred pred) : pred(pred) {}

	template<typename Stream>
	auto build(Stream stream) {
		throw_if_endless(stream);
		while(stream.next())
			if(std::invoke(pred, stream.front()))
				return true;
		return false;
	}
};

template<typename Pred>
struct NoneMatchBuilder {
	Pred pred;
	
	explicit NoneMatchBuilder(Pred pred) : pred(pred) {}

	template<typename Stream>
	auto build(Stream stream) {
		throw_if_endless(stream);
		while(stream.next())
			if(std::invoke(pred, stream.front()))
				return false;
		return true;
	}
};

template<typename Counter>
struct CountBuilder {
	Counter counter;
	
	explicit CountBuilder(Counter counter) : counter(counter) {}
	
	template<typename Stream>
	auto build(Stream stream) {
		throw_if_endless(stream);
		while(stream.next()) ++counter;
		return counter;
	}
};

template<typename First>
auto from_endless_iterator(First first) {
	return EndlessIteratorStream(first);
}

template<typename First, typename Last>
auto from_iterator(First first, Last last) {
	return IteratorStream(first, last, false);
}

template<typename Container>  
auto from(Container const& container) {
	return from_iterator(std::begin(container), std::end(container));
}

template<typename First, typename Last>
auto from_unchecked_iterator(First first, Last last) {
	return IteratorStream(first, last, true);
}

template<typename Container>  
auto from_unchecked(Container const& container) {
	return from_iterator(std::begin(container), std::end(container));
}


template<typename IntType>
auto iota(IntType const& first, IntType const& step = 1) {
	return IntegerStream<IntType>(first, step);
}

template<typename Getter>
auto generate(Getter getter) {
	return GenerateStream(getter);
}

template<typename Pred>  
auto filter(Pred pred) { return builder_of<FilterStream>(pred); }

template<typename Pred>  
auto map(Pred pred) { return builder_of<MapStream>(pred); }

auto take(size_t count) { return make_builder([count](auto stream){ return TakeStream(stream, count);}); };

auto skip(size_t count) { return make_builder([count](auto stream){ return SkipStream(stream, count);}); };

template<typename Pred>
auto take_while(Pred pred) { return builder_of<TakeWhileStream>(pred); };

template<typename Pred>
auto skip_while(Pred pred) { return builder_of<SkipWhileStream>(pred); };

template<typename Init, typename Pred>
auto iterate(Init init, Pred pred) {
	return GenerateStream([=, first = true] () mutable {return first ? first = false, init : init = pred(init);});
}

template<typename Init, typename Pred0, typename Pred>
auto iterate(Init init, Pred0 pred0, Pred pred) {
	return iterate(init, pred) >> take_while(pred0);
}

template<typename IntType>
auto int_range(IntType const& last) {
	return iota(IntType(0), IntType(1)) >> take(last);
}
template<typename IntType>
auto int_range(IntType const& first, IntType const& last) {
	return iota(first, IntType(1)) >> take(last - first);
}

template<typename IntType>
auto int_range(IntType const& first, IntType const& last, IntType const& step) {
	return iota(first, step) >> take((last - first + IntType(1)) / step);
}

template<typename T>
auto endless_empty_stream() {
	return EndlessEmptyStream<T>();
}

template<typename T>
auto empty_stream() {
	return EmptyStream<T>();
}

template<typename T>
auto endless_singleton(std::nullopt_t) {
	return endless_empty_stream<T>();
}

template<typename T>
auto endless_singleton(std::optional<T>&& opt) {
	return generate([=]{ return opt; }) >> filter([](auto opt){ return opt.has_value(); }) >> map([](auto opt){ return opt.value(); });
}

template<typename T>
auto singleton(std::nullopt_t) {
	return empty_stream<T>();
}

template<typename T>
auto singleton(std::optional<T>&& opt) {
	return generate([=]{ return *opt; }) >> take(opt.has_value());
}

auto sort() {
	return builder_of<SortStream>(std::less<>{});
}

template<typename Compare>
auto sort(Compare compare) {
	return builder_of<SortStream>(compare);
}

auto reverse() { return make_builder([](auto stream){ return throw_if_endless(stream), ReverseStream(stream); }); }

template<typename Set>
auto distinct(Set&& set) { return builder_of<DistinctStream>(set); }

template<typename Peeker>
auto peek(Peeker peeker) { return builder_of<PeekStream>(peeker); }

auto endless_flat() { return builder_of<EndlessFlatStream>(); }

auto flat() { return builder_of<FlatStream>(); }

auto make_endless() { return builder_of<MakeEndlessStream>(); }

auto tail_repeat() { return builder_of<TailRepeatStream>(); }

auto loop() { return builder_of<LoopStream>(); }

template<typename ...Streams>
auto join_streams(Streams... streams) {
	static_assert(sizeof...(Streams) >= 2);
	return JoinStreams<std::remove_reference_t<Streams>...>(std::move(streams)...);
} 
template<typename Pred, typename ...Streams>
auto combine_streams(Pred pred, Streams... streams) {
	static_assert(sizeof...(Streams) >= 2);
	return CombineStreams<std::remove_reference_t<Streams>...>(pred, std::move(streams)...);
} 

IterableBuilder iterable() { return {}; }

template<typename Pred>
auto for_each(Pred pred) { return ForEachBuilder(pred); }

auto first() { return make_builder([](auto stream){ return stream.next() ? std::optional(stream.front()) : std::nullopt; }); }

template<typename BiPred>
auto reduce(BiPred biPred) { return ReduceBuilder(biPred); }

auto min() { return MinBuilder(std::less<>{}); }

auto max() { return MaxBuilder(std::less<>{}); }

auto minmax() { return MinMaxBuilder(std::less<>{}); }

template<typename Compare>
auto min(Compare compare) { return MinBuilder(compare); }

template<typename Compare>
auto max(Compare compare) { return MaxBuilder(compare); }

template<typename Compare>
auto minmax(Compare compare) { return MinMaxBuilder(compare); }

template<typename Pred>
auto all_match(Pred pred) { return FindAllBuilder(pred); }

template<typename Pred>
auto any_match(Pred pred) { return FindAnyBuilder(pred); }

template<typename Pred>
auto none_match(Pred pred) { return FindNoneBuilder(pred); }

template<typename Counter>
auto count(Counter counter) { return CountBuilder(counter); }

template<typename Container, typename Collector>
auto collect(Container container, Collector collector) {
	return make_builder([=](auto stream) mutable {
		stream >> for_each(std::bind(collector, std::ref(container), std::placeholders::_1));
		return container;
	});
}

auto element_at(size_t pos) {
	return make_builder([pos](auto stream){ return stream >> skip(pos) >> first(); });
}

template<typename Pred>
auto endless_flat_map(Pred pred) {
	return make_builder([pred](auto stream){ return map(pred).build(std::move(stream)) >> endless_flat(); });
}

template<typename Pred>
auto flat_map(Pred pred) {
	return make_builder([pred](auto stream){ return map(pred).build(std::move(stream)) >> flat(); });
}

namespace type_erasure {

template<typename T>
struct StreamHolderBase {
	using value_type = T;
	virtual ~StreamHolderBase() {}
#ifndef YAO_STREAM_NO_TYPEINFO
	virtual const std::type_info& type() const = 0; 
#endif
	virtual StreamHolderBase* clone() const = 0;
	virtual value_type front() = 0;
	virtual bool next() = 0;
	virtual bool endless() const = 0;
};
template<typename Stream>
struct StreamHolder : StreamHolderBase<valueType<Stream>> {
	using base_type = StreamHolderBase<valueType<Stream>>;
	Stream stream;
	
	StreamHolder(Stream const& stream) : stream(stream) {}

#ifndef YAO_STREAM_NO_TYPEINFO
	virtual const std::type_info& type() const override {
		return typeid(stream);
	}
#endif

	virtual base_type* clone() const override {
		return new StreamHolder(stream);
	}
	
	virtual typename base_type::value_type front() override {
		return stream.front();
	}
	
	virtual bool next() override {
		return stream.next();
	}
	
	virtual bool endless() const override {
		return stream.endless();
	}
};
template<typename T>
struct AnyStream {
	StreamHolderBase<T>* holder;
	
	using value_type = T;
	
	AnyStream() : holder(nullptr) {}
	
	template<typename Stream>
	AnyStream(Stream const& stream) 
		: holder(new StreamHolder(stream)) {}
	
	AnyStream(AnyStream const& other)
		: holder(other.holder ? other.holder->clone() : nullptr) {}
		
	AnyStream(AnyStream && other)
		: holder(std::exchange(other.holder, nullptr)) {}
	
	template<typename Stream>
	AnyStream& operator=(Stream const& stream) {
		delete holder;
		holder = new StreamHolder(stream);
		return *this;
	}
	
	AnyStream& operator=(AnyStream const& other) {
		delete holder;
		holder = other.holder ? other.holder->clone() : nullptr;
		return *this;
	}
	
	AnyStream& operator=(AnyStream && other) {
		delete holder;
		holder = std::exchange(other.holder, nullptr);
		return *this;
	}
	
#ifndef YAO_STREAM_NO_TYPEINFO
	const std::type_info& type() const {
		return holder->type();
	}
#endif
	
	decltype(auto) front() {
		return holder->front();
	}
	
	bool next() {
		return holder->next();
	}
	
	bool endless() const {
		return holder->endless();
	}
	
	Buildable;
	
	~AnyStream() {
		delete holder;
	}
	
};
template<typename Stream>
AnyStream(Stream const& stream) -> AnyStream<valueType<Stream>>;
}

} 
#undef Buildable
#endif
