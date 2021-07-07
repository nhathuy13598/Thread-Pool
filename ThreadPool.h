#include <thread>
#include <cstddef>
#include <future>
#include <condition_variable>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <functional>
#include <atomic>
class Threadpool {
private:
    std::vector<std::thread> _threadpools;
    std::condition_variable _cv;
    std::condition_variable _cvQueueFull;
    bool _stop;
    bool _pause;
    std::queue<std::packaged_task<void()>> _tasks;
    mutable std::mutex _lock;
    std::unique_ptr<std::promise<bool>> _final_task;
    size_t _maxQueuedTasks;
private:
    void listen()
    {
        while (true) {
            std::packaged_task<void()> task;
            {
                std::unique_lock<std::mutex> lock(_lock);
                while(!_stop && ( _pause || _tasks.empty())) {
                    _cv.wait(lock);
                }
                // TODO: Find ways to not wait to run all remaining tasks
                if (true == _stop && _tasks.empty())
                    break;
                task = std::move(_tasks.front());
                _tasks.pop();
                lock.unlock();
                _cvQueueFull.notify_one();
            }
            task();
        }
        std::lock_guard<std::mutex> lock(_lock);
        if (_final_task)
        {
            _final_task->set_value(true);
            _final_task = nullptr;
        }
    }
    void finish_task()
    {
        std::unique_lock<std::mutex> lock(_lock);
        _stop = true;
        lock.unlock();
        _cv.notify_all();
    }
public:
    Threadpool() = delete;
    Threadpool(const Threadpool&) = delete;
    Threadpool(Threadpool&&) = delete;
    Threadpool& operator=(const Threadpool&) = delete;
    Threadpool& operator=(Threadpool&&) = delete;

    explicit Threadpool(int max_threads, size_t max_task = 2000) :
        _stop(false),
        _pause(false),
        _threadpools(max_threads),
        _maxQueuedTasks(max_task)
    {
        for (size_t i = 0; i < max_threads; i++)
        {
            _threadpools[i] = std::move(std::thread(std::bind(&Threadpool::listen, this)));
        }
    }

    ~Threadpool() {
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

    bool is_stopped() const {
        std::lock_guard<std::mutex> lock(_lock);
        return _stop;
    }

    void stop() {
        std::unique_lock<std::mutex> lock(_lock);
        _stop = true;
        lock.unlock();
        _cv.notify_all();
    }

    void pause() {
        std::lock_guard<std::mutex> lock(_lock);
        _pause = true;
    }

    void resume() {
        std::unique_lock<std::mutex> lock(_lock);
        _pause = false;
        lock.unlock();
        _cv.notify_all();
    }

    std::future<bool> finish() {
        static std::atomic_bool once = false;
        if (!once.exchange(true))
        {
            _final_task = std::make_unique<std::promise<bool>>();
            auto final_task_future = _final_task->get_future();
            this->add_task([this]() {this->finish_task();});
            return final_task_future;
        }
        auto dummy = std::promise<bool>();
        dummy.set_value(false);
        return dummy.get_future();
    }

    /**
     * @brief Add new task to queue. If users add task when Threadpool is stopped 
     * then it will throw exception.
     * @tparam Fn Function
     * @tparam Args Variable arguments
     * @param fn Function name
     * @param args Arguments
     * @return std::future<std::result_of_t<Fn(Args...)>> Future of packaged_task
     */
    template<typename Fn, typename... Args>
    std::future<std::result_of_t<Fn(Args...)>> add_task(Fn&& fn, Args&&... args) {
        using return_type = std::result_of_t<Fn(Args...)>;
        std::packaged_task<return_type()> callable_task(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));
        std::future<return_type> result = callable_task.get_future();

        std::unique_lock<std::mutex> lock(_lock);
        // TODO: Find ways to not throw exception
        if (true == _stop)
            throw std::runtime_error("try to add new task on stopped TaskManager");
        while (_tasks.size() >= _maxQueuedTasks)
        {
            _cvQueueFull.wait(lock);
        }
        _tasks.push(std::packaged_task<void()>(std::move(callable_task)));
        lock.unlock();
        _cv.notify_one();
        return result;
    }
};
