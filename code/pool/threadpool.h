
//实际就是一个生产者消费者模型，很简单！
//注意共享资源锁，线程同步

//面对对象的思想，并且是c++11新标准

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>   //c++11自己的创建线程，和linux有点不一样
#include <functional>
class ThreadPool {
public:
    // 消费者！
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
    //explicit关键字，防止构造函数发生隐式转换
            assert(threadCount > 0);    //断言 测试

            // 创建threadCount个子线程
            for(size_t i = 0; i < threadCount; i++) {
                std::thread([pool = pool_] {
                    std::unique_lock<std::mutex> locker(pool->mtx); //锁
                    while(true) {   // 创建好线程后，死循环，让子线程在等待队列不断取任务
                        if(!pool->tasks.empty()) {  // 如果线程池不为空
                            auto task = std::move(pool->tasks.front()); // move移动语义
                            // 从线程池头部取一个任务
                            pool->tasks.pop();  // 移除掉
                            locker.unlock();    // 对mtx这个共享数据解锁
                            task(); // 进行任务
                            locker.lock();
                        } 
                        else if(pool->isClosed) break;  // 当线程池是关闭状态，break 当前线程的while
                        else pool->cond.wait(locker);   // 阻塞
                    }
                }).detach();    // 设置线程分离，不需要父线程去释放
            }
    }

    ThreadPool() = default; // default使用默认的实现
    //如果程序员为类 X 显式的自定义了非默认构造函数，却没有定义默认构造函数的时候，将会出现编译错误：
    //原因在于类 X 已经有了用户自定义的构造函数，所以编译器将不再会为它隐式的生成默认构造函数。
    //如果需要用到默认构造函数来创建类的对象时，程序员必须自己显式的定义默认构造函数。

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() { // 析构函数
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true; // 关闭线程池
            }
            pool_->cond.notify_all();   // 把所有的线程唤醒，可能有线程还在睡眠
        }
    }

    // 生产者!
    template<class F>   // 模板
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();   // 唤醒一个睡眠的线程
    }

private:
    // 结构体，池子
    struct Pool {
        std::mutex mtx; // 互斥锁
        std::condition_variable cond;   // 条件变量
        bool isClosed;      // 是否关闭
        std::queue<std::function<void()>> tasks;    // 队列（保存的是任务）
    };
    std::shared_ptr<Pool> pool_;    // 这就是一个现场池
};


#endif //THREADPOOL_H