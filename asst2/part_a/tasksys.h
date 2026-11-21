#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <pthread.h>
#include <atomic>

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial: public ITaskSystem {
    public:
        TaskSystemSerial(int num_threads);
        ~TaskSystemSerial();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn: public ITaskSystem {
    public:
        TaskSystemParallelSpawn(int num_threads);
        ~TaskSystemParallelSpawn();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();

    private:
        int num_threads_;
        static void* workerStatic(void* arg);
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();

    private:
        struct LaunchState {
            IRunnable* runnable;
            int num_total_tasks;
            int chunk_size;
            std::atomic<int> next_task;
            std::atomic<int> num_done;
        };

        int num_threads_;
        pthread_t* threads_;                    // 常驻线程池
        std::atomic<bool> done_;                // 析构时通知 worker 退出
        std::atomic<LaunchState*> current_launch_; // NULL = 空闲
        std::vector<LaunchState*> launches_;    // 已完成的 launch，延后到析构统一释放
        static void* workerStatic(void* arg);
        // 领取并执行一个 chunk；返回 false 表示本 launch 的任务已被领完。
        bool tryRunChunk(LaunchState* l);
        // 常驻 worker 数 = num_threads_ - 1，主线程补上最后一个执行上下文。
        int workerCount() const { return num_threads_ > 0 ? num_threads_ - 1 : 0; }
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();

    private:
        std::atomic<int> sleeping_workers; // 记录当前正在睡觉的 worker 数量
        pthread_mutex_t state_mtx = PTHREAD_MUTEX_INITIALIZER;
        pthread_cond_t  sleep_cond = PTHREAD_COND_INITIALIZER;
        pthread_cond_t  done_cond = PTHREAD_COND_INITIALIZER;

        struct LaunchState {
            IRunnable* runnable;
            int num_total_tasks;
            int chunk_size;
            std::atomic<int> next_task;
            std::atomic<int> num_done;
        };

        int num_threads_;
        pthread_t* threads_;                    // 常驻线程池
        std::atomic<bool> done_;                // 析构时通知 worker 退出
        std::atomic<LaunchState*> current_launch_; // NULL = 空闲
        std::vector<LaunchState*> launches_;    // 已完成的 launch，延后到析构统一释放
        static void* workerStatic(void* arg);
};

#endif
