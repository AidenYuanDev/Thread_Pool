@[toc]

# 一、前言
C++后端开发线程池是绕不开的话题。今天带大家写一个更加现代的线程池。
一句话总结：这是一个实现了任务调度与执行的C++线程池，支持动态添加任务并通过多个线程并行处理。

项目地址如下，如有帮助点点star！
[ThreadPool](https://github.com/AidenYuanDev/Thread_Pool)
# 二、前置知识

## 1`thread`、`mutex`、`condition_variable`
这些是最基础的并发知识就不多赘诉了。

## 2 `future`
为什么要用`future`
> 一方面来讲，可以异步访问操作的结果了。但是这不是主要原因，主要原因是我们要封装成`packaged_task`，这个的返回对象是这个，所以用这个。

### 2.1 定义
它提供了一种从异步操作中获取结果的机制。它是 C++ 并发编程中的核心组件之一，通常与 std::async、std::packaged_task 或 std::promise 一起使用。
### 2.2 头文件
```c
#include <future>
```
### 2.3 基本概念
- 表示一个尚未就绪的结果。
- 提供了一种机制来访问异步操作的结果。

### 2.4 主要特性
- 允许异步获取操作结果。
- 支持超时和轮询。
- 可以传递异常。

### 2.5 主要成员函数
- **get()：** 获取结果，如果结果还没准备好会阻塞。
- **wait()：** 等待结果变为可用，但不获取结果。
- **wait_for()：** 等待指定的时间段。
- **wait_until()：** 等待直到指定的时间点。
- **valid()：** 检查 future 对象是否有效（是否关联了共享状态）。

### 2.6 与其他组件的配合
- **std::async：** 自动创建 future。
- **std::packaged_task：** 通过 get_future() 获取 future。
- **std::promise：** 通过 get_future() 获取关联的 future。

## 3、`std::packaged_task`
### 3.1 基本概念
- 将一个可调用对象 **(这可以是一个函数、lambda表达式或函数对象)** 包装成一个可以异步执行的任务。
- 提供了一种机制来存储任务的结果，并在未来某个时间点获取这个结果。

```c
std::packaged_task<ReturnType(ArgTypes...)> task(callable);
```

### 3.2 为什么用`std::packaged_task`
可以用`bind`函数，吧函数和可变参数都封装成`packaged_task`，实现接口的统一。

## 4 可变参数
### 4.1 定义
可变参数（variadic parameters）允许函数接受可变数量的参数。

~~~c
template<typename... Args>
void functionName(Args... args) {
    // 函数体
}
~~~

### 4.2 包展开

~~~c
template<typename... Args>
void printAll(Args... args) {
    (std::cout << ... << args) << std::endl;
}

printAll(1, 2.0, "three"); // 输出：12three
~~~

## 5 `std::bind`
C++11 引入的一个函数模板，它用于创建一个新的可调用对象，将现有的可调用对象与一组参数绑定在一起。

### 5.1 头文件
```c
#include <functional>
```
### 5.2 基本语法
~~~c
auto new_function = std::bind(callable, arg1, arg2, ..., argN);
~~~

## 6 `std::function`
C++11 引入的一个强大的可调用对象包装器，它可以存储、复制和调用任何可调用目标，包括普通函数、Lambda 表达式、函数对象和成员函数。

> 主要的应用就是结合lambda表达式一起，实现对所以函数的统一接口
>  functional<void()>
### 6.1 头文件
```c
#include <functional>
```
### 6.2基本语法
~~~c
std::function<return_type(parameter_types...)> func_name;
~~~

## 7 完美转发 = 引用折叠 + 万能引用 + `std::forward`

### 7.1 为什么需要完美转发
**保持参数类型信息：** 直接使用引用传递可能会丢失参数的一些重要特性，比如是否为const，是左值还是右值。
举个例子来说明问题：
~~~c
#include <iostream>
#include <utility>

class Widget {
public:
    Widget(int n) { std::cout << "构造函数" << std::endl; }
    Widget(const Widget& w) { std::cout << "拷贝构造" << std::endl; }
    Widget(Widget&& w) { std::cout << "移动构造" << std::endl; }
};

// 左值引用版本
template<typename T>
void createAndProcess1(T& arg) {
    Widget w(arg);
}

// const左值引用版本
template<typename T>
void createAndProcess2(const T& arg) {
    Widget w(arg);
}

// 右值引用版本
template<typename T>
void createAndProcess3(T&& arg) {
    Widget w(std::move(arg));
}

int main() {
    Widget w1(1);

    std::cout << "左值引用版本：" << std::endl;
    createAndProcess1(w1);  // 可以编译，调用拷贝构造
    // createAndProcess1(Widget(2));  // 无法编译

    std::cout << "\nconst左值引用版本：" << std::endl;
    createAndProcess2(w1);  // 可以编译，调用拷贝构造
    createAndProcess2(Widget(2));  // 可以编译，调用拷贝构造

    std::cout << "\n右值引用版本：" << std::endl;
    // createAndProcess3(w1);  // 无法编译
    createAndProcess3(Widget(2));  // 可以编译，调用移动构造

    return 0;
}
~~~
上面的问题汇总
- 左值引用版本无法处理右值。
- const左值引用版本可以处理所有情况，但无法利用移动语义。
- 右值引用版本无法处理左值。

使用完美转发就可以解决这个问题

### 7.1 基本概念
完美转发使用 **右值引用（&&）** 和 **std::forward<T>()** 函数模板来实现。

### 7.2 引用折叠

这里实参、形参就算调换位置也一样，画多了乱。

|实参  | 形参 |结果 |
|--|--|--|
|T&  | & |T& |
|T&  | && |T& |
|`T&&`  | & |T& |
|`T&&`  | && |T&& |

> 上面的结果符合口诀 `遇左则左`

由最后两行，可以引出万能引用的概念

### 7.3 万能引用
使用 `T&&` 作为形参

简单来说，不管你传入的是左值引用还是右值引用，最后都会折叠成相对于的左值引用还是右值引用，引用类型不变。

### 7.4 `std::forward`完美转发
#### 7.4.1 万能引用存在的问题
**左值的定义：** 在C++中，左值通常是指有名字、可以取地址的表达式。简单来说，如果一个表达式有持久的身份（可以被引用多次），它就是左值。

> 简单来说，虽然万能引用使右值引用绑定到右值引用（参数类型没有变化），但是他实际上在函数体内是一个左值，因为

- 参数 有一个名字
- 可以对 参数取地址
- 可以多次使用 参数

#### 7.4.2  `std::forward` 解决这个问题
到这日常的开发就没有问题了，记住下面的套路就行。

~~~c
template<typename T>
void createAndProcess(T&& arg) {
    Widget w(std::forward<T>(arg)); // 传到其他函数使用std::forward 来实现完美转发
}
~~~

#### 7.4.3 `std::forward`的具体实现
~~~c
template<typename T>
T&& forward(typename std::remove_reference<T>::type& param) {
    return static_cast<T&&>(param);
}

template<typename T>
T&& forward(typename std::remove_reference<T>::type&& param) {
    return static_cast<T&&>(param);
}
~~~

1. **std::remove_reference<T>::type：** 移除类型 T 中的引用。

举个例子
- **在左值情况下：**
T 被推导为 int&
std::remove_reference<T>::type 是 int
函数参数类型是 int&（第一个重载版本）
- **在右值情况下：**
T 被推导为 int
std::remove_reference<T>::type 是 int
函数参数类型是 int&&（第二个重载版本）

2.  **static_cast<T&&> :** 转换成万能引用

举个例子
- 如果 T 是左值引用（如 int&），T&& 会折叠成左值引用 int&
- 如果 T 是非引用类型（如 int），T&& 会变成右值引用 int&&

# 三、线程池实现
首先，明确一个点，线程池是要给不同的函数提供线程的，所以要用泛型编程来支持。

**线程池结构**  

-  创建线程池。
- 把函数封装成任务，添加到任务队列。

1. **创建线程池：**  线程池就是创建一定数量的线程（阻塞着获取任务且执行一个任务不会退出（死循环））来抢夺任务队列的任务去执行。

2. **把函数封装成任务，添加到任务队列。** 用`bind`把函数指针和及可变参数封装成`packaged_task`任务，加到任务队列中(这个操作相当于生产者，所以记得加锁)。

>  注意操作任务队列的时候，要加锁就行了。

下面看是实现

~~~c
#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>
#include <functional>
#include <future>
using namespace std;

class ThreadPool {
public:
    explicit ThreadPool(const int thread_num);
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ~ThreadPool();

    template <typename F, typename... Args>
    auto enqueue(F &&f, Args &&...agrs)
        -> future<decltype(f(agrs...))>;

private:
    void worker_thread();

private:
    vector<thread> workers;
    queue<function<void()>> tasks;

    mutex queue_mutex;
    condition_variable condition;
    atomic<bool> stop; // 线程池是否销毁，是为了让任务队列的任务全部执行完毕
};

ThreadPool::ThreadPool(const int thread_num): stop(false) {
    workers.reserve(thread_num);
    for (int i = 1; i <= thread_num; i++) {
        // emplace_back 构造一个线程
        workers.emplace_back(&ThreadPool::worker_thread, this);
    } 
}

ThreadPool::~ThreadPool() {
    {
        unique_lock<mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (auto& worker : workers) worker.join();
}

void ThreadPool::worker_thread() {
    // 无线循环，使之运行完一个任务不会退出
    while (true) {
        function<void()> task;
        // 取出任务，作用域加锁，消费者模型（条件变量）
        {
            unique_lock<mutex> lock(queue_mutex);
            condition.wait(lock, [this] {return stop || !tasks.empty();});
            if (stop && tasks.empty()) return;
            // move 提高效率
            task = std::move(tasks.front());
            tasks.pop();
        }
        // 执行取出的任务
        task();
    }
}
template <typename F, typename... Args>
auto ThreadPool::enqueue(F &&f, Args &&...args)
    -> future<decltype(f(args...))> {
    // 推导 参数绑定到函数指针 的返回类型 decltype(f(args...)
    using return_type = decltype(f(args...));
    //  使用共享指针，让他能用 function<void()> 实现统一的接口，也叫类型擦除
    auto task = make_shared<packaged_task<return_type()>>(
        bind(std::forward<F>(f), std::forward<Args>(args)...));

    future<return_type> res = task->get_future();
    {
        unique_lock<mutex> lock(queue_mutex);
        if (stop) runtime_error("enqueue on stopped ThreadPool");
        tasks.emplace([task] {(*task)();});
    }
    return res; // 返回 future
}

void show(int id) {
    cout << "id: " << id << endl; 
}

int main(){
    ThreadPool pool(5);
    vector<future<int>> ress;

    for (int i = 1; i <= 10; i++) {
        // 把show函数指针 和 参数 i 加入任务队列
        pool.enqueue(show, i);
    }

    for (auto &res : ress) res.get(); // 异步获取任务的结果
    return 0;
}
~~~
