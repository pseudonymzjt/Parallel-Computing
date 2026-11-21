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
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemSerial::sync() {
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
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
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

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): 
    ITaskSystem(num_threads),
    num_threads_(num_threads),
    workers(num_threads)
{
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    for(int i = 0; i < num_threads; i++) {
        pthread_create(&workers[i], NULL, workerStatic, this);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    pthread_mutex_lock(&mtx);
    end = true;
    pthread_cond_broadcast(&worker_cond);
    pthread_mutex_unlock(&mtx);
    for(int i = 0; i < num_threads_; i++) {
        pthread_join(workers[i], NULL);
    }
    for (auto& p: launches) delete p.second;
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    runAsyncWithDeps(runnable, num_total_tasks, {});
    sync(); // very important to wait this launch done!
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //
    pthread_mutex_lock(&mtx);
    TaskID new_id = next_launch++;
    launchState* l = new launchState();
    l->id = new_id;
    l->runnable = runnable;
    l->num_total_tasks = num_total_tasks;
    l->chunk_size = std::max(1, num_total_tasks / (num_threads_ * 4));
    l->in_degree = 0;
    
    /* dependent noted if and only if the launch appears in deps and hasn't finished */
    for(TaskID id : deps) {
        auto it = launches.find(id);
        if(it != launches.end() && !it->second->isFinished) {l->in_degree++; it->second->dependents.push_back(l);}
    }

    launches[new_id] = l;
    remain_launches++;

    if(l->in_degree == 0) {
        if(num_total_tasks > 0) {ready_launches.push_back(l); pthread_cond_broadcast(&worker_cond);}
        else {l->isFinished = true; remain_launches--;}
    }

    pthread_mutex_unlock(&mtx);

    return new_id;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //
    pthread_mutex_lock(&mtx);
    /* wait till all launches done */
    while(remain_launches) {
        pthread_cond_wait(&done_cond, &mtx); // atomically release the lock and hang done_cond on the main thread
    }
    pthread_mutex_unlock(&mtx);
    return;
}

void* TaskSystemParallelThreadPoolSleeping::workerStatic(void* arg) {
    TaskSystemParallelThreadPoolSleeping* ts =
        static_cast<TaskSystemParallelThreadPoolSleeping*>(arg);
    
    while(true) {
        pthread_mutex_lock(&ts->mtx);
        // ready list is empty and not ended, wait till awakened
        while(ts->ready_launches.empty() && !ts->end) pthread_cond_wait(&ts->worker_cond, &ts->mtx);
        // ended, defensively exit
        if(ts->end && ts->ready_launches.empty()) {pthread_mutex_unlock(&ts->mtx); return nullptr;}

        // retrieve a task and deque the soon-to-finished launch
        launchState* l = ts->ready_launches.front();
        ts->ready_launches.pop_front();
        int begin = l->next_task;
        l->next_task += l->chunk_size;
        int end = std::min(l->next_task, l->num_total_tasks);
        if(end < l->num_total_tasks) {
            ts->ready_launches.push_back(l); // avoid blocking a long dependant chain
            // Nudge one sleeping worker to help grab the remaining chunks of this launch.
            // Harmless when everyone is already awake (the signal is then a no-op).
            pthread_cond_signal(&ts->worker_cond);
        }
        pthread_mutex_unlock(&ts->mtx);

        /* real parallel part */
        for(int i = begin; i < end; i++) {
            l->runnable->runTask(i, l->num_total_tasks);
        }

        pthread_mutex_lock(&ts->mtx);
        int tasks_done = end - begin;
        if((l->num_done += tasks_done) >= l->num_total_tasks) {
            l->isFinished = true;
            int newTasks = 0;

            for(launchState* child : l->dependents) {
                if(--child->in_degree == 0) {
                    if(child->num_total_tasks > 0) {
                        ts->ready_launches.push_back(child);
                        // careful that there's no need to increment remain_launches
                        newTasks++;
                    }
                    else {
                        child->isFinished = true;
                        ts->remain_launches--;
                    }
                }
            }

            if(newTasks) pthread_cond_broadcast(&ts->worker_cond);
        
            if(!(--ts->remain_launches)) pthread_cond_broadcast(&ts->done_cond);
        }
        pthread_mutex_unlock(&ts->mtx);
    }
}