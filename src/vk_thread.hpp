#ifndef VK_THREAD_H
#define VK_THREAD_H

#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>

namespace vke {

class Thread
{
public:
    Thread();
    ~Thread();

    void addJob(std::function<void()> job);
    void wait();
    void stop();

private:
    void run();

    bool destroying = false;
    std::thread worker;
    std::queue<std::function<void()>> jobQueue;
    std::mutex queueMutex;
    std::condition_variable condition;
};

} // end namespace vke

#endif // VK_THREAD_H
