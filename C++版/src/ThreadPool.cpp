#include "ThreadPool.h"

ThreadPool::ThreadPool(int min, int max) {
    //实例化任务队列
    taskQ = new TaskQueue;
    do {
        threadIDs = new pthread_t[max];
        if (threadIDs == nullptr) {
            cout << "malloc threadIDs fail..." << endl;
            break;
        }
        minNum = min;
        maxNum = max;
        busyNum = 0;
        liveNum = min;
        exitNum = 0;

        if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
                pthread_cond_init(&notEmpty, NULL) != 0) {
            cout << "mutex or condition init dail..." << endl;
            break;
        }

        shutdown = false;

        //创建线程
        pthread_create(&managerID, NULL, manager, this);
        for (int i = 0; i < min; i++) {
            pthread_create(&threadIDs[i], NULL, worker, this);
        }
        return;
    }
    while(false);

    //释放资源
    if (threadIDs)  delete[] threadIDs;
    if (taskQ)  delete taskQ;
}

ThreadPool::~ThreadPool(){
    //关闭线程池
    shutdown = true;
    //阻塞回收管理者线程
    pthread_join(managerID, NULL);
    for (int i = 0; i < liveNum; i++) {
        pthread_cond_signal(&notEmpty);
    }
    //释放内存
    if (taskQ)  delete taskQ;
    if (threadIDs)  delete[] threadIDs;

    pthread_mutex_destroy(&mutexPool);
    pthread_cond_destroy(&notEmpty);
} 

int ThreadPool::getBusyNum() {
    pthread_mutex_lock(&mutexPool);
    int busyNum = this->busyNum;
    pthread_mutex_unlock(&mutexPool);
    return busyNum;
}

int ThreadPool::getAliveNum() {
    pthread_mutex_lock(&mutexPool);
    int liveNum = this->liveNum;
    pthread_mutex_unlock(&mutexPool);
    return liveNum;
}

void ThreadPool::addTask(Task task) {
    if (shutdown) return;

    //添加任务
    taskQ->addTask(task);

    pthread_cond_signal(&notEmpty);
}

void ThreadPool::threadExit() {
    pthread_t tid = pthread_self();
    for (int i = 0; i < maxNum; i++) {
        if (threadIDs[i] == tid) {
            threadIDs[i] = 0;
            cout << "threadExit() called, " << to_string(tid) << "exiting..." << endl;
            break;
        }
    }
    pthread_exit(NULL);
}

void* ThreadPool::worker(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    while(true) {
        pthread_mutex_lock(&pool->mutexPool);
        while(pool->taskQ->taskNumber() == 0 && !pool->shutdown) {
            //阻塞工作线程
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

            //判断是不是要销毁线程
            if (pool->exitNum > 0) {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum) {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    pool->threadExit();
                }
            }
        }
        
        //线程池是否关闭
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutexPool);
            pool->threadExit();
        }
    
        //从任务队列中取出一个任务
        Task task = pool->taskQ->takeTask();
        pool->busyNum++;
        
        //解锁
        pthread_mutex_unlock(&pool->mutexPool);
        
        cout << "thread" << to_string(pthread_self()) << "start working..." << endl;
       
        task.function(task.arg);
        task.arg = nullptr;
        
        //任务结束
        cout << "thread" << to_string(pthread_self()) << "end working..." << endl;

        pthread_mutex_lock(&pool->mutexPool);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexPool);
    }
}

void* ThreadPool::manager(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while (!pool->shutdown) {
        //每隔3s检测一次
        sleep(3);

        //取出线程池中任务的数量、当前线程的数量、当前忙的线程数量
        pthread_mutex_lock(&pool->mutexPool);
        int taskNumber = pool->taskQ->taskNumber();
        int liveNum = pool->liveNum;
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexPool);
       
       //添加线程
       //任务的个数>存活的线程个数 && 存活的线程数<最大线程数
        if (taskNumber > liveNum && liveNum < pool->maxNum) {
            pthread_mutex_lock(&pool->mutexPool);
            for (int i = 0, counter = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; i++) {
                if (pool->threadIDs[i] == 0) {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    counter++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        } 

        //销毁线程
        //忙的线程*2 < 存活的线程数 && 存活的线程 > 最小线程数
        if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            //让工作的线程自杀
            for (int i = 0; i < NUMBER; i++) {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return NULL;
}
