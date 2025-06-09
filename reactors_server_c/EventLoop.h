#pragma once
#include "Dispatcher.h"

extern struct Dispatcher EpoolDispatcher;
struct EventLoop{
    Dispatcher* dispatcher;
    void* dispatcherData;
};