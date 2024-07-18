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
