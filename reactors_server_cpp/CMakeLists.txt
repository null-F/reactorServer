cmake_minimum_required(VERSION 3.14)
project(ReactorServer)

set(CMAKE_CXX_STANDARD 14)

# 找到 Threads（即 pthread）
find_package(Threads REQUIRED)

add_executable(ReactorHttpCpp 
        main.cpp 
        Channel.cpp 
        Channel.h 
        Dispatcher.cpp 
        Dispatcher.h 
        EpollDispatcher.cpp 
        EpollDispatcher.h 
        PollDispatcher.cpp 
        PollDispatcher.h 
        EventLoop.cpp 
        EventLoop.h 
        SelectDispatcher.cpp 
        SelectDispatcher.h
        WorkerThread.cpp
        WorkerThread.h
        ThreadPool.cpp
        ThreadPool.h
        ThreadPool.h
        TcpServer.cpp
        TcpServer.h
        Log.h
        Buffer.cpp
        Buffer.h
        HttpRequest.cpp
        HttpRequest.h
        HttpResponse.cpp
        HttpResponse.h
        TcpConnection.cpp
        TcpConnection.h
        TcpConnection.h)

# 链接 pthread
target_link_libraries(ReactorHttpCpp PRIVATE Threads::Threads)