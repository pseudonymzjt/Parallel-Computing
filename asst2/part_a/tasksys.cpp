#include "tasksys.h"


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    this->num_threads_ = num_threads;
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}


typedef struct {
    IRunnable* runnable;
    int thread_id;
    int num_threads;
    int num_total_tasks;
} WorkerArgs;

void* TaskSystemParallelSpawn::workerStatic(void* worker_args) {
    WorkerArgs* args = static_cast<WorkerArgs*>(worker_args);
    for (int id = args->thread_id;
         id < args->num_total_tasks;
         id += args->num_threads) {
        args->runnable->runTask(id, args->num_total_tasks);
    }
    return NULL;
}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) {
        return;
    }

    int num_threads = std::min(num_total_tasks, num_threads_);
    pthread_t* workers = new pthread_t[num_threads > 1 ? num_threads - 1 : 0];
    WorkerArgs* args = new WorkerArgs[num_threads];

    for (int i = 0; i < num_threads; i++) {
        args[i].runnable = runnable;
        args[i].thread_id = i;
        args[i].num_threads = num_threads;
        args[i].num_total_tasks = num_total_tasks;
    }

    // 调用线程承担一个分片，减少每次 run() 的 pthread 创建开销。
    for (int i = 1; i < num_threads; i++) {
        pthread_create(&workers[i - 1], NULL, workerStatic, &args[i]);
    }
    workerStatic(&args[0]);
    for (int i = 1; i < num_threads; i++) {
        pthread_join(workers[i - 1], NULL);
    }

    delete[] workers;
    delete[] args;
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

namespace {
// 自旋等待时让出流水线，减少同一 cache line 上的争用。
// 在 16 线程下 worker 数 + 主线程会超过可用核数，纯热自旋会导致
// 刚被调度上的工作线程立刻又被抢占。
inline void relaxSpin() {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#else
    sched_yield();
#endif
}
}  // namespace

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads)
    : ITaskSystem(num_threads),
      num_threads_(num_threads),
      threads_(new pthread_t[num_threads]),
      done_(false),
      current_launch_(NULL) {
    // 只创建 workerCount() 个常驻 worker，最后一个执行上下文留给主线程，
    // 避免出现 (worker 数 + 主线程) > 核数的超订，超订时自旋会互相抢占。
    for (int i = 0; i < workerCount(); i++) {
        pthread_create(&threads_[i], NULL, workerStatic, this);
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    done_.store(true);
    for (int i = 0; i < workerCount(); i++) {
        pthread_join(threads_[i], NULL);
    }
    delete[] threads_;
    for (size_t i = 0; i < launches_.size(); i++) {
        delete launches_[i];
    }
}

bool TaskSystemParallelThreadPoolSpinning::tryRunChunk(LaunchState* l) {
    int begin = l->next_task.fetch_add(
        l->chunk_size,
        std::memory_order_relaxed);

    if (begin >= l->num_total_tasks) {
        return false;   // 任务已被领完
    }

    int end = std::min(begin + l->chunk_size, l->num_total_tasks);
    for (int id = begin; id < end; id++) {
        l->runnable->runTask(id, l->num_total_tasks);
    }

    int completed = end - begin;
    if (l->num_done.fetch_add(
            completed,
            std::memory_order_relaxed) + completed
        == l->num_total_tasks) {
        current_launch_.store(NULL, std::memory_order_release);
    }
    return true;
}

void* TaskSystemParallelThreadPoolSpinning::workerStatic(void* arg) {
    TaskSystemParallelThreadPoolSpinning* ts =
        static_cast<TaskSystemParallelThreadPoolSpinning*>(arg);
    LaunchState* exhausted_launch = NULL;

    while (true) {
        if (ts->done_.load(std::memory_order_acquire)) {
            return NULL;
        }

        LaunchState* l =
            ts->current_launch_.load(std::memory_order_acquire);
        if (l == NULL || l == exhausted_launch) {
            relaxSpin();
            continue;
        }

        if (!ts->tryRunChunk(l)) {
            // 当前 launch 的任务都已被领走；等下次 launch 再参与抢任务。
            exhausted_launch = l;
        }
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) {
        return;
    }

    LaunchState* l = new LaunchState();
    l->runnable = runnable;
    l->num_total_tasks = num_total_tasks;
    l->chunk_size = std::max(
        1, num_total_tasks / (num_threads_ * 4));
    l->next_task.store(0, std::memory_order_relaxed);
    l->num_done.store(0, std::memory_order_relaxed);

    // 上一次 launch 结束时状态已被清空；这里只是保险。
    while (current_launch_.load(std::memory_order_acquire) != NULL) {
        relaxSpin();
    }

    current_launch_.store(l, std::memory_order_release);
    launches_.push_back(l);

    // 主线程也是执行上下文：自己抢任务，同时负责等待整轮结束。
    while (current_launch_.load(std::memory_order_acquire) == l) {
        if (!tryRunChunk(l)) {
            relaxSpin();   // 任务已被领完，等其他 worker 收尾
        }
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads),
      sleeping_workers(0),
      num_threads_(num_threads),
      threads_(new pthread_t[num_threads]),
      done_(false),
      current_launch_(NULL)
{
    for (int i = 0; i < num_threads_; i++) {
        pthread_create(&threads_[i], NULL, workerStatic, this);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    // 必须在 state_mtx 保护下设置 done_ 并广播 sleep_cond：
    // worker 在持锁状态下检查 done_ 并进入 pthread_cond_wait，若这里不持锁，
    // 广播可能落在 worker 真正进入等待之前而丢失，导致 pthread_join 永久卡住。
    pthread_mutex_lock(&state_mtx);
    done_.store(true);
    pthread_cond_broadcast(&sleep_cond);
    pthread_mutex_unlock(&state_mtx);
    for (int i = 0; i < num_threads_; i++) {
        pthread_join(threads_[i], NULL);
    }
    delete[] threads_;
    for (size_t i = 0; i < launches_.size(); i++) {
        delete launches_[i];
    }
}

void* TaskSystemParallelThreadPoolSleeping::workerStatic(void* arg) {
    TaskSystemParallelThreadPoolSleeping* ts =
        static_cast<TaskSystemParallelThreadPoolSleeping*>(arg);

    while (true) {
        if (ts->done_.load(std::memory_order_acquire)) {
            return NULL;
        }

        LaunchState* l = ts->current_launch_.load(std::memory_order_acquire);

        // 统一处理两种“没活干”的情况：1. 根本没有 launch；2. 当前 launch 任务已被领光
        if (l == NULL || l->next_task.load(std::memory_order_relaxed) >= l->num_total_tasks) {
            pthread_mutex_lock(&ts->state_mtx);
            // 持有锁后必须二次确认条件，防止 Lost Wakeup
            while (!ts->done_.load(std::memory_order_acquire)) {
                LaunchState* cur = ts->current_launch_.load(std::memory_order_acquire);
                if (cur != NULL && cur->next_task.load(std::memory_order_relaxed) < cur->num_total_tasks) {
                    break; // 发现新任务
                }
                pthread_cond_wait(&ts->sleep_cond, &ts->state_mtx);
            }
            pthread_mutex_unlock(&ts->state_mtx);
            continue; // 醒来后回到循环开头重新 load 最新的 current_launch_
        }

        // 尝试领取一个 chunk 的任务
        int begin = l->next_task.fetch_add(l->chunk_size, std::memory_order_relaxed);
        if (begin >= l->num_total_tasks) {
            // 刚刚被别的线程抢光了，回到循环顶部走统一的睡眠逻辑
            continue;
        }

        int end = std::min(begin + l->chunk_size, l->num_total_tasks);
        for (int id = begin; id < end; id++) {
            l->runnable->runTask(id, l->num_total_tasks);
        }

        int completed = end - begin;
        if (l->num_done.fetch_add(completed, std::memory_order_acq_rel) + completed == l->num_total_tasks) {
            // 必须在 state_mtx 保护下置空并唤醒主线程，防止 run() 错过 signal
            pthread_mutex_lock(&ts->state_mtx);
            ts->current_launch_.store(NULL, std::memory_order_release);
            pthread_cond_signal(&ts->done_cond);
            pthread_mutex_unlock(&ts->state_mtx);
        }
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    if (num_total_tasks <= 0) {
        return;
    }

    LaunchState* l = new LaunchState();
    l->runnable = runnable;
    l->num_total_tasks = num_total_tasks;
    l->chunk_size = std::max(1, num_total_tasks / (num_threads_ * 4));
    l->next_task.store(0, std::memory_order_relaxed);
    l->num_done.store(0, std::memory_order_relaxed);

    pthread_mutex_lock(&state_mtx);
    while (current_launch_.load(std::memory_order_acquire) != NULL) {
        pthread_cond_wait(&done_cond, &state_mtx);
    }

    current_launch_.store(l, std::memory_order_release);
    launches_.push_back(l);
    pthread_cond_broadcast(&sleep_cond); // 唤醒所有睡觉的 Worker

    while (current_launch_.load(std::memory_order_acquire) != NULL) {
        pthread_cond_wait(&done_cond, &state_mtx);
    }
    pthread_mutex_unlock(&state_mtx);
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
