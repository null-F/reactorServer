#include "ThreadPool.h"
#include <assert.h>
#include <stdlib.h>

// 初始化线程池
struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int threadNum) {
    struct ThreadPool* pool = (struct ThreadPool*)malloc(sizeof(struct ThreadPool));
    pool->mainLoop = mainLoop; // 主线程的反应堆模型
 
    pool->index = 0;
    pool->isStart = false;
    pool->threadNum = threadNum; // 子线程总个数
    pool->workerThreads = (struct WorkerThread*)malloc(sizeof(struct WorkerThread) * threadNum); // 子线程数组
    return pool;
}

// 启动线程池
void threadPoolRun(struct ThreadPool* pool)
{
    //线程池创建好之后就要让线程池中的若干子线程运行起来
    //确保线程池未运行
    assert(pool && !pool->isStart);
    // 比较主线程的ID和当前线程ID是否相等 
    // 相等=>确保执行线程为主线程；不相等=>exit(0)
    if (pool->mainLoop->threadID != pthread_self()){
        exit(0);
    }
    pool->isStart = true; // 标记为启动
    if(pool->threadNum > 0){
        for(int i = 0; i < pool->threadNum; ++i){
            workerThreadInit(&pool->workerThreads[i],i); //初始化子线程
            workerThreadRun(&pool->workerThreads[i]);
        }
    }
}                                

// 取出线程池中某个子线程的反应堆实例
/*
    这个函数是主线程调用的，因为主线程是拥有线程池的。
    因此主线程可以遍历线程池里边的子线程，从中挑选一个子线程，
    得到它的反应堆模型，再将处理的任务放到反应堆模型里边
*/
struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool)
{
    assert(pool->isStart); // 确保线程池处于运行状态
    // 比较主线程的ID和当前线程ID是否相等 
    // 相等=>确保执行线程为主线程；不相等=>exit(0)
    if(pool->mainLoop->threadID != pthread_self()){
        exit(0);
    }
    //从线程池中找到一个子线程，然后取出反应堆实例
    struct EventLoop* evLoop = pool->mainLoop;
    if(pool->threadNum > 0){
        evLoop = pool->workerThreads[pool->index].evLoop;
        pool->index = ++pool->index % pool->threadNum;
    }
    return evLoop;
}