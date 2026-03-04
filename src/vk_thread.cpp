#include "vk_thread.hpp"

using namespace vke;

Thread::Thread()
{
    worker = std::thread(&Thread::run, this);
}

Thread::~Thread()
{
    if (worker.joinable()) {
        wait();
        stop();

        worker.join();
    }
}

void Thread::addJob(std::function<void()> job)
{
    std::lock_guard<std::mutex> lock(queueMutex);
    jobQueue.push(std::move(job));
    condition.notify_one();
}

void Thread::wait()
{
    std::unique_lock<std::mutex> lock(queueMutex);
    condition.wait(lock, [this]() { return jobQueue.empty(); });
}

void Thread::stop()
{
    std::lock_guard<std::mutex> lock(queueMutex);
    destroying = true;
    condition.notify_one();
}

void Thread::run()
{
    while (true) {
        std::function<void()> job;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return !jobQueue.empty() || destroying; });

            if (destroying)
                break;

            job = jobQueue.front();
        }

        job();

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            jobQueue.pop();
            condition.notify_one();
        }
    }
}
