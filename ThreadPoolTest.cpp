#include <iostream>
#include <chrono>
#include "ThreadPool.h"
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
    Threadpool manager(5, 2);

    manager.add_task<int(*)(int), int>(test, 1);
    manager.add_task<int(*)(int), int>(test, 2);
    manager.add_task<int(*)(int), int>(test, 3);
    manager.add_task<int(*)(int), int>(test, 4);
    manager.add_task<int(*)(int), int>(test, 5);
    manager.pause();
    std::this_thread::sleep_for(2s);
    manager.resume();
    return 0;
}