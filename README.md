# cppStream
### ——by Yaossg

## 简介

cppStream灵感来源于Java的Stream API，使用C++17实现

## 简单演示

	using namespace yao::stream;

	auto result = int_range(1, 101)
		>> reduce(std::plus<>{});
	std::cout << result.value() << std::endl; // 5050


这是一个最简单的例子，`int_range(1, 101)`生成一个整数流，`reduce(plus<>{})`对它进行求和，并返回一个`optional<int>`，的到结果5050，中间用运算符`>>`连接

cppStream的所有组件都在`yao::stream`命名空间下

如果定义了宏`CPP_STREAM_NO_EXCEPTION`，那么这个库将直接终止程序，而不是抛出异常

## 流是什么

就是流水线，与容器不同，延时求值是流最大的特点，只有需要求值，才回去求值

### 流的分类

* 流分为有限流和无限流，其中某些无限流是可能无限的无限流，仍然属于无限流，想要检查一个流是否无限，可以调用`stream.endless()`来检查。

* 流也可以分为源和中间流，源就是流中数据的源头，详见下面获取流一节，中间流就是处理流数据的部分，详见下面中间操作一节

### 流的底层操作
#### 成员类型

	using value_type = ...;

值类型，表示这个流包含的元素的类型

	template<T>
	using value_type_t = ...;

辅助模板，`value_type_t<T>`相当于`typename T::value_type`

#### 成员函数

	decltype(auto) front();

对于流的第一个元素进行访问，在不调用`next()`的情况下`front() == front()`恒为`true`

	bool next();

检查流是否还有下一个元素，应该总是在`front()`调用之前进行检查

	bool endless() const;

检查这个流是否是一个无限流

	template<typename Builder>
	decltype(auto) operator>>(Builder builder) const;

使用`Builder`构建下一个流

**[提示]** 不调用`next()`而直接调用`front()`结果是未定义的(UB)。

**[提示]** 当`!next()`时调用`front()`，结果是未定义的(UB)，对于一个无限流进行有限操作，会抛出`endless_stream_exception`异常，对于一个无限流进行无限操作，结果是未定义的(UB)。

**[提示]** 如果需要自定义，后面会讲到如何使用这些类型和函数，但一般而言不要直接调用他们。

## 获取流

	from_endless_iterator(First first)

生成一个无限流，流中元素就是`*first++`得到的。

	from_iterator(First first, Last last)
	from(Container const& container)

生成一个有限流，流中包含`[first, last)`中所有元素，如果first至last是无法到达的(unreachable)，结果是未定义的(UB)，因此再调用之前，应该确保他已经被检查。

	from_unchecked_iterator(First first, Last last)
	from_unchecked(Container const& container)

生成一个无限流，即使first至last是可以到达的(reachable)，流中包含`[first, last)`中所有元素。

	generate(Getter getter)

生成一个无限流，每一个元素都是通过`getter()`获得的。

	iterate(Init init, Pred pred)

生成一个无限流，其中第一个元素是`init`，剩下的每个元素都是`init =  pred(init)`。

	iterate(Init init, Pred0 pred0, Pred pred)

生成一个(可能)无限流，其中第一个元素是`init`，剩下的每个元素都是`init =  pred(init)`，直到`!pred0(init)`(不包含)。

	iota(IntType first, IntType step = 1)

生成一个无限流，流中的元素类型是`IntType`，其中第一个元素是`first`，剩下的元素是上一个元素`+step`。

	int_range(IntType last)

生成一个有限流，流中的元素类型是`IntType`，流中元素是`[0, last)`。

	int_range(IntType first, IntType last)
	int_range(IntType first, IntType last, IntType step)

生成一个有限流，流中的元素类型是`IntType`，流中元素是`[first, last)`，最后每两数之间差值为`step`。

	empty_stream()
	singleton(std::nullopt_t)

生成一个空流，对它调用`front()`结果是未定义的(UB)。

	singleton(std::optional<T>&& opt)

生成只包含一个元素的流，如果`!opt.has_value()`，则生成一个空流。

	endless_empty_stream()
	endless_singleton(std::nullopt_t)

生成一个无限空流，对它调用`next()`和`front()`结果都是未定义的(UB)。

	endless_singleton(std::optional<T>&& opt)

生成只包含一个重复元素的无限流，如果`!opt.has_value()`，则生成一个无限空流。

## 中间操作

	filter(Pred pred)

过滤流中的元素，过滤一切`!pred(elemenet)`的元素。如果一个无限流没有一个元素满足这个`pred`，结果是未定义的。

	map(Pred pred)

映射流中的操作，流中的元素从`element`变为`pred(element)`。

	take(size_t count)

截短流，流的流仅剩前`count`个元素，会把流变为有限流。

	skip(size_t count)

跳过前`count`个元素。

	take_while(Pred pred)

截短流，只要满足`!Pred(element)`，就不在向后读取，包括首个不满足该条件的元素。不会改变流的性质，无限流仍然是无限流。

	skip_while(Pred pred)

跳过所有元素直到满足`!pred(element)`，不包括这个元素。

	sort()
	sort(Compare compare)

为流进行稳定排序(stable sort)，默认的比较器是`std::less<>{}`。

如果这是一个无限流，将会抛出`endless_stream_exception`

	reverse()

颠倒流中元素的顺序。如果这是一个无限流，将会抛出`endless_stream_exception`。

	distinct(Set&& set)

去掉流中重复的元素，使用提供的`set`作为缓存器。

	peek(Peeker peeker)

偷窥流中的元素，流中每个元素都会调用`peek(element)`，只要它被求值。

	endless_flat()
	flat()

扁平化流，前者得到一个无限流，后者在遇到无限流是抛出`endless_stream_exception`。

 [未完成：关于"扁平化"的解释]

	endless_flat_map(Pred pred)
	flat_map(Pred pred)

` >> [endless_]flat(pred)`相当于` >> map(pred) >> [endless_]flat()`。

	make_endless()

使这个流的`endless()`返回`true`。

	tail_repeat()

当流耗尽，会用最后一个元素作为之后的元素，生成一个无限流。

	loop()

当流耗尽，从头开始，如果这是一个空流，结果是未定义的(UB)。

## 复合操作

复合操作不使用`operator>>`，而是直接把流作为函数的参数。

	join_streams(Streams... streams)

连接流，把streams...中的流顺次连接，这些流的类型不必一致，如果其中一个流是无限流，后面的流的元素可能永远不被求值。

	combine_streams(Pred pred, Streams... streams)

组合流，吧streams...中流的每个元素调用`pred(elements...)`组成一个新的流，这个流的长度取决于最长的那个流，只有所有流都是无限的，生成的流才是无限的。

## 终端操作

	first()
	element_at(size_t pos)

获取第一个元素，或者获取指定位置的元素

	for_each(Pred pred)

对于每个元素执行`pred(element)`。如果这是一个无限流，将会抛出`endless_stream_exception`。

	reduce(BiPred biPred)

规约，以第一个元素作为`init`，不断执行`init = biPred(init, element)`。如果这是一个无限流，将会抛出`endless_stream_exception`。

	min()
	min(Compare compare)
	max()
	max(Compare compare)

获得最大值或最小值，默认`compare`是`std::less<>{}`。如果这是一个无限流，将会抛出`endless_stream_exception`。

	minmax()
	minmax(Compare compare)

获得一个pair，first是最小值，second是最大值，默认`compare`是`std::less<>{}`。如果这是一个无限流，将会抛出`endless_stream_exception`。

	all_match(Pred pred)
	any_match(Pred pred)
	none_match(Pred pred)

使用`pred(element)`匹配全部/任意/没有元素，返回bool。

	count(Counter counter)

计算流的长度，Counter必须重载了前缀`++`。如果这是一个无限流，将会抛出`endless_stream_exception`。

	collect(Container container, Collector collector)

收集流中元素。如果这是一个无限流，将会抛出`endless_stream_exception`。

 [未完成：关于"收集"的解释]

## 类型擦除

所有有关类型擦除的组件都在`yao::stream::type_erasure`命名空间下

如果你不需要`<typeinfo>`中的组件支持你可以定义`CPP_STREAM_NO_TYPEINFO`宏来取消

### AnyStream<T\>

可以存任何元素类型为`T`的流，拥有和其他流一样的接口

额外多一个`type()`函数，只有在没定义`CPP_STREAM_NO_TYPEINFO`宏时才可以使用，返回存的流的类型信息

可以不指定`T`，库提供了自动推导：

	type_erasure::AnyStream range = int_range(0, 100); //自动类型推导

## 并行

### 警告！本库不负责任何形式的线程安全措施，请自己做好安全措施！

所有有关并行的组件都在`yao::stream::parallel`命名空间下

### 工具类模板SplitStream<>

 [未完成]

### 并行版本的终端操作

	for_each(std::uintmax_t task_size, Pred pred)
	reduce(std::uintmax_t task_size, BiPred biPred)
	min(std::uintmax_t task_size)
	min(std::uintmax_t task_size, Compare compare)
	max(std::uintmax_t task_size)
	max(std::uintmax_t task_size, Compare compare)
	minmax(std::uintmax_t task_size)
	minmax(std::uintmax_t task_size, Compare compare)
	all_match(std::uintmax_t task_size, Pred pred)
	any_match(std::uintmax_t task_size, Pred pred)
	none_match(std::uintmax_t task_size, Pred pred)
	count(std::uintmax_t task_size, Counter counter)
	collect(std::uintmax_t task_size, Container container, Collector collector)

除了多了第一个参数task_size，接受的参数没有区别

总结

 [未完成]
