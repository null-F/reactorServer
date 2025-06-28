#include "WorkerThread.h"
#include <stdio.h>

// 子线程的回调函数
void WorkerThread::running()
{
    m_mutex.lock();
    m_evLoop = new EventLoop(m_name);
    m_mutex.unlock();
    m_cond.notify_one();
    m_evLoop->run();
}

WorkerThread::WorkerThread(int index)
{
    m_evLoop = nullptr;
    m_thread = nullptr;
    m_threadID = thread::id();
    m_name =  "SubThread-" + to_string(index);
}

WorkerThread::~WorkerThread()
{
    if (m_thread != nullptr)
    {
        delete m_thread;
    }
}

void WorkerThread::run()
{
    // 创建子线程
    m_thread = new thread(&WorkerThread::running, this);
    // 阻塞主线程, 让当前函数不会直接结束
    unique_lock<mutex> locker(m_mutex);
    /*
    unique_lock 是一个类模板，它封装了互斥锁
    unique_lock 可以在构造时不立即获取锁，而是通过调用 lock() 方法来延迟获取锁
    unique_lock 与条件变量（condition_variable）配合使用时，允许线程在等待条件变量时暂时释放锁，并在条件变量被唤醒后重新获取锁
    */
    while (m_evLoop == nullptr)
    {
        m_cond.wait(locker);
    }
}
