#include <thread>
#include <cstddef>
#include <future>
#include <condition_variable>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <functional>
class TaskManager {
public:
    enum policy {
        launch = 0,
        defer
    };
protected:
    std::vector<std::thread> _threadpools;
    std::condition_variable _cv;
    bool _stop;
    bool _pause;
    std::queue<std::packaged_task<void()>> _tasks;
    std::mutex _lock;
    int _cur_running_tasks;
    int _max_running_tasks;
protected:
    void listen()
    {
        while (true) {
            std::packaged_task<void()> task;
            {
                std::unique_lock<std::mutex> lock(_lock);
                _cv.wait(lock, [this]() -> bool {
                    return true == _stop || (false == _tasks.empty() && _cur_running_tasks < _max_running_tasks && false == _pause);
                });
                if (true == _stop && true == _tasks.empty())
                    break;
                task = std::move(_tasks.front());
                _tasks.pop();
                _cur_running_tasks++;
            }
            task();
            {
                std::unique_lock<std::mutex> lock(_lock);
                _cur_running_tasks--;
            }
        }
    }
public:
    TaskManager() = delete;
    TaskManager(const TaskManager&) = delete;
    TaskManager(TaskManager&&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    TaskManager& operator=(TaskManager&&) = delete;

    explicit TaskManager(int max_threads, int max_running_tasks) {
        _threadpools.resize(max_threads);
        for (size_t i = 0; i < max_threads; i++)
        {
            _threadpools[i] = std::move(std::thread(std::bind(&TaskManager::listen, this)));
        }
        _cur_running_tasks = 0;
        _max_running_tasks = max_running_tasks;
        _stop = false;
        _pause = false;
    }

    ~TaskManager() {
        std::unique_lock<std::mutex> lock(_lock);
        _stop = true;
        lock.unlock();
        _cv.notify_all();
        for (size_t i = 0; i < _threadpools.size(); i++) {
            if (true == _threadpools[i].joinable()) {
                _threadpools[i].join();
            }
        }
    }

    void stop() {
        std::unique_lock<std::mutex> lock(_lock);
        _stop = true;
        lock.unlock();
        _cv.notify_all();
    }

    void pause() {
        std::unique_lock<std::mutex> lock(_lock);
        _pause = true;
    }

    void resume() {
        std::unique_lock<std::mutex> lock(_lock);
        _pause = false;
        lock.unlock();
        _cv.notify_all();
    }

    void increase_task(size_t num_task) {
        std::unique_lock<std::mutex> lock(_lock);
        _max_running_tasks += num_task;
        lock.unlock();
        _cv.notify_all();
    }

    void decrease_task(size_t num_task) {
        std::unique_lock<std::mutex> lock(_lock);
        if (num_task > _max_running_tasks)
            throw std::runtime_error("try to decrease more than _max_running_tasks");
        _max_running_tasks -= num_task;
        lock.unlock();
    }

    void increase_thread(size_t num_thread) {
        for (size_t i = 0; i < num_thread; i++) {
            _threadpools.push_back(std::thread(std::bind(&TaskManager::listen, this)));
        }
    }

    void start_deferred_tasks() {
        std::unique_lock<std::mutex> lock(_lock);
        lock.unlock();
        _cv.notify_all();
    }

    template<typename Fn, typename... Args>
    std::future<std::result_of_t<Fn(Args...)>> add_task(TaskManager::policy type, Fn&& fn, Args&&... args) {
        using return_type = std::result_of_t<Fn(Args...)>;
        std::packaged_task<return_type()> callable_task(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
        std::future<return_type> result = callable_task.get_future();
        std::unique_lock<std::mutex> lock(_lock);
        if (true == _stop)
            throw std::runtime_error("try to add new task on stopped TaskManager");
        _tasks.push(std::packaged_task<void()>(std::move(callable_task)));
        lock.unlock();
        if (TaskManager::policy::launch == type)
        {
            _cv.notify_one();
        }
        return result;
    }
};
