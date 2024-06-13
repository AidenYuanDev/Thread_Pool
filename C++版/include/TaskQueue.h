#pragma once
#include <iostream>
using namespace std;
#include <queue>
#include <pthread.h>
#include <stdarg.h>
#include <string>
#include <unistd.h>
using callback = void (*)(void* arg);

struct Task {
    Task() {
        function = nullptr;
        arg = nullptr;
    }
    Task(callback f, void* arg) {
        this->function = f;
        this->arg = arg;
    }
    callback function;
    void* arg;
};

class TaskQueue {
  public:
    TaskQueue();
    ~TaskQueue();

    //添加任务
    void addTask(Task task);
    void addTask(callback f, void* arg);
    //取出一个任务
    Task takeTask();
    //获取当前任务个数
    size_t taskNumber() {
        return m_taskQ.size();
    }

  private:
    queue<Task> m_taskQ;
    pthread_mutex_t m_mutex;
};
