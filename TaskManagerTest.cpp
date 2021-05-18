#include <iostream>
#include <chrono>
#include "TaskManager.h"
using namespace std;
using namespace std::chrono;
std::mutex global;
int test(int a)
{
    std::lock_guard<std::mutex> lock(global);
    std::cout << std::this_thread::get_id() << ": " << a << std::endl;
    return a;
}
int main()
{
    TaskManager manager(5, 2);

    manager.add_task<int(*)(int), int>(TaskManager::policy::defer, test, 1);
    manager.add_task<int(*)(int), int>(TaskManager::policy::defer, test, 2);
    manager.add_task<int(*)(int), int>(TaskManager::policy::defer, test, 3);
    manager.add_task<int(*)(int), int>(TaskManager::policy::defer, test, 4);
    manager.add_task<int(*)(int), int>(TaskManager::policy::defer, test, 5);
    manager.start_deferred_tasks();
    manager.pause();
    std::this_thread::sleep_for(2s);
    manager.resume();
    while(true)
    {
        std::this_thread::sleep_for(10s);
        manager.stop();
    }
    return 0;
}