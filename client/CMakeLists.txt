
add_executable(client main.cpp net.cpp net.h)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(client PRIVATE Threads::Threads)