## 模块功能概述

<img src="https://i-blog.csdnimg.cn/blog_migrate/474c5d45159be9d02c76aee20794df7d.png" alt="img" style="zoom:70%;" />

### 反应堆模型部分

首先，反应堆（Reactor）：核心为回调

Reactor 模式是一种基于事件驱动的模式，应用程序并不是主动去调用某些API来完成处理，相反，应用程序需要提供相应的接口并注册到Reactor上，如果相应的事件被触发，Reactor将主动调用应用程序注册的接口——接口又称之为【回调函数】

用于高效地处理大量的并发 I/O 操作。其核心思想是通过一个或多个反应堆来监听和分发 I/O 事件，将不同的事件分发给相应的处理器进行处理

拓展：两种高效的事件处理模式（reactor模式、proactor模式）后续补充一下proactor.......

### Channel模块

<img src="C:\Users\PC\Pictures\image\image-20250609140623258.png" alt="image-20250609140623258" style="zoom:50%;" />

```c
************************Channel模块结构*********************
//定义文件描述符的读写事件
enum FDEvent{
    TimeOut = 0x01,
    ReadEvent = 0x02,
    WriteEvent = 0x04
};

struct Channel{
    int fd; //文件描述符（主线程反应堆对应lfd，子线程反应堆对应cfd）
    int events; //事件
    handleFunc readCallback; //读回调
    handleFunc writeCallback; //写回调
    handleFunc destroyCallback; // 销毁回调
    void* arg; //回调函数参数
};
关于上图中data的解释：如果说我调用了一个读回调或者调用了一个写回调，这个读写回调函数是不是有可能有参数？关于这个参数应该很好理解，在执行一段代码的过程中，很可能需要动态数据。这个动态数据怎么来的呢？就是通过参数传递进来的，也就是arg
```

```c
[拓展——函数指针]	在c++中可以通过模板函数来实现这一功能
typedef int(*handleFunc)(void* arg);
在C语言中，函数指针是一种指针，它指向函数的地址。函数指针类型就是指这种指针的类型，它描述了指针所指向的函数具有什么参数和返回值。使用函数指针类型，可以让我们将函数作为参数传递给其他函数，或者将函数存储在数组中，实现更加灵活和动态的编程
#include <stdio.h>
// 定义一个函数指针类型
typedef int (*func_ptr_type)(int);
// 定义一个函数，接受一个整数参数并返回其平方
int square(int x) {
    return x * x;
}
// 定义一个函数，接受一个函数指针作为参数，并调用该函数
int apply_func(func_ptr_type func, int x) {
    return func(x);
}
int main() {
    // 声明一个函数指针变量，并指向 square 函数
    func_ptr_type ptr = square;
    
    // 通过函数指针调用 square 函数
    printf("%d\n", ptr(5)); // 输出：25
    
    // 将 square 函数作为参数传递给 apply_func 函数
    printf("%d\n", apply_func(square, 6)); // 输出：36
    
    return 0;
}
```

**IO多路**模型：允许同时对多个文件描述符进行操作，当某个文件描述符对应的事件被触发时，系统会调用相应的事件处理函数

**事件检测**：基于IO多路模型，当文件描述符对应的事件被触发时，系统会检测到该事件

总结：Channel模块的封装主要包括文件描述符、事件检测和回调函数。在服务器端，Channel主要用于封装文件描述符，用于监听和通信。事件检测是基于IO多路模型的，当文件描述符对应的事件被触发时，会调用相应的事件处理函数。在Channel结构中，需要指定读事件和写事件对应的回调函数。此外，还有一个data参数用于传递动态数据。在判断事件方面，对于通信的文件描述符，不能去掉写事件的检测，否则无法接收客户端数据。可以通过添加或删除文件描述符对应的写事件来实现对客户端新链接的检测。判断写事件检测的函数isWriteEventEnable用于判断参数Channel里边儿对应的事件里是否有写事件的检测。在实现Channel模块的封装时，需要定义三个API函数：初始化Channel的函数、修改文件描述符的写事件函数和判断写事件检测的函数。这些函数的实现相对简单，主要是对结构体中的各个数据成员进行初始化或修改。实现Channel的WriteEventEnable，首先需要判断第二个参数flag是否等于true。如果等于true，则追加写属性；如果等于FALSE，则去掉写属性。具体的实现方式可以学习C语言里边的通用做法，通过判断标志位来确定是否有写事件。例如，可以定义读事件为100，写事件为10，通过读取标志位来判断是否有写事件。如果标志位为1，则表示有写事件；如果标志位为0，则表示没有写事件。

### ChannelMap模块

ChannelMap其实就是为了Channel来服务的，在Channel中已经封装了文件描述符，当我i们得到某个文件描述符时并对其对应的事件进行处理，此时就需要找到这个文件描述符对应的channel（channel里面包含了事件对应的回调函数），此时就可以通过**数组**（在c++中可以通过**哈希表**来实现）来实现，即每个数组下标对应一个文件描述符，value值对应channel，如下图

<img src="C:\Users\PC\Pictures\image\image-20250618211010314.png" alt="image-20250618211010314" style="zoom:67%;" />

```c
struct ChannelMap{
    struct Channel** list; //指针数组
    int size; //记录指针指向的数组的元素总个数
};

void ChannelMapClear(struct ChannelMap* map) {
    if(map != NULL) {
        for(int i=0;i<map->size;++i) {
            if(map->list[i] != NULL) {
                free(map->list[i]);
            }
        }
        free(map->list);
        map->list = NULL;
    }
    map->size=0;
}
这里在写清空map的函数时候有个小的点——关于两次free的内容不同，并且有明确的先后顺序，不能弄错
    终端会出现：已放弃，核心转存————由于非法内存访问导致的
    注意这里两次free不是同一个东西，第一次就看成了同一个，以为是内存重复释放了，如果真写成了一个，应该会导致内存泄漏
    导致的问题主要有：错误释放内存导致内存泄漏；重复释放内存导致程序崩溃；释放后访问无效内存导致程序崩溃
```

把文件描述符 fd 和 channel 的对应关系存储到 channelMap。这么做是为了什么？

在 dispatcher 里边，还有 dispatch 函数指针，也就是 dispatcher->dispatch(evLoop,timeout) 。这个是一个检测函数，通过调用 dipatch 函数，就可以得到激活的文件描述符，得到了激活的文件描述符之后，需要通过这个文件描述符找到它所对应的 channel

### Dispatcher模块

该模块是反应堆模型中的核心部分，因为在反应堆模型里边有事件需要处理或者检测，都要通过poll、epoll、select来完成，并且这三个是互斥的，只选其一。不管使用哪一个，都可以往这个模型里边添加一个新的待检测事件，或者说把一个已经检测的事件从这个检测模型里边删掉。还有一种情况，就是把一个已经被检测得到文件描述符它的事件进行修改，比如原来是读事件，现在改成读写

![image-20250618213351558](C:\Users\PC\Pictures\image\image-20250618213351558.png)

```c
struct Dispatcher {
    /*  
        【拓展——泛函数（指针函数）】
        init 初始化epoll、select、或者poll需要的数据块
        最后需要把这个数据块的内存地址给到函数的调用者
        所以它的返回值肯定是一个指针(因为是不同的数据类型、c++中就可以用模板函数来实现)
        另外epoll、select、或者poll它们需要的数据块对应的内存类型一样吗？
        不一样，如果想要一种类型来兼容三种不同的类型，怎么做到呢？
        在C语言里就是使用泛型，故返回值类型为void*
    */
    void* (*init)();
    // 添加
    int (*add)(struct Channel* channel,struct EventLoop* evLoop);
    // 删除
    int (*remove)(struct Channel* channel,struct EventLoop* evLoop);
    // 修改
    int (*modify)(struct Channel* channel,struct EventLoop* evLoop);
    // 事件检测
    int (*dispatch)(struct EventLoop* evLoop,int timeout); // 单位:s
    // 清除数据（关闭fd或者释放内存）
    int (*clear)(struct EventLoop* evLoop);
};

dispatch():用于事件检测的，对于poll来说，就是调用poll函数，对于epoll来说，就是调用epoll_wait函数，对于select来说，就是调用select函数。通过调用dispatch函数就能够知道检测的这一系列的文件描述符集合里边到底是哪一个文件描述符它所对应的事件被触发了，找到了这个被触发事件的文件描述符，就需要基于它的事件去调用文件描述符注册好的读函数或者是写函数了
上述定义的这6个函数，最后都需要在slect、poll、epoll对应的dispatcher中进行实例化，从而将这三个模型里边的函数实现之后，就看用户选择，从而实现使用dispatch这个结构体里边的这些函数指针的名字，就可以对下边这些已经实现了的函数进行调用了
```

一个Dispatcher模型，它对应一个DispatcherData，它们都同时存在于另一个模块里边（EventLoop）,是一个对应关系。我们如果想把这个Dispatcher对应的data取出来，那么就需要通过EventLoop来取了，所以要得到EventLoop的地址之后，也就能拿到这个Dispatcher对应的DispatcherData了

在我们要实现的这个多[反应堆](https://so.csdn.net/so/search?q=反应堆&spm=1001.2101.3001.7020)服务器模型里边，**Dispatcher**一共有多少个？这个EventLoop里边， 其实就有Dispatcher，这个Dispatcher就是事件分发器，这个事件分发器其实就是要编写的那个poll、 epoll 或者select模块，我们在实现Dispatcher它底层的这三个模型里边，任意一个的时候都需要一个DispatcherData。这么一看，其实需要n个，项目里边有多少个反应堆模型，它就有多少个**EventLoop**，那么底层就有多少个**Dispatcher**。***\*Dispatcher\****有多少个，那么这个***\*DispatcherData\****就有多少个。所以，需要给底层的这个IO多路转接模型提供对应的数据块，有多少个多路lO转接模型，就需要提供多少个***\*DispatcherData\****。通过以上分析，我们可以看到**dispatcher**结构体在系统中的核心作用。它不仅定义了**lO**多路转接模型所需的数据块，还提供了初始化这些数据的函数。而**EventLoop**结构体则为**dispatcher**提供了一个工作平台，确保了数据的正确使用和管理。

![image-20250619134921474](C:\Users\PC\Pictures\image\image-20250619134921474.png)

```c
struct EpollData{
    int epfd;
    struct epoll_event* events; // 数组指针
};

//EpollDispatcher 是 Dispatcher 的一个实例
struct Dispatcher EpollDispatcher = {
    epollInit,
    epollAdd,
    epollRemove,
    epollModify,
    epollDispatch,
    epollClear
};

poll与slect同理，项目中的三个Dispatcher的c文件主要就是对这三个不同的模块的六个函数进行了实例化
```

关于epoll的理论知识看篇[博客](https://subingwen.cn/linux/epoll/#2-%E6%93%8D%E4%BD%9C%E5%87%BD%E6%95%B0)，关于slect与poll看这篇[博客](https://subingwen.cn/linux/select/)

文件描述符与 Channel 在 Dispatcher 中的交互

- Channel 结构体：封装了文件描述符，并在其中存储了所需的数据
- 添加文件描述符到 Dispatcher ：当有新文件描述符需要添加时，它会被放入 Dispatcher 的检测集合中
- 文件描述符激活与处理：如果某个文件描述符在 Dispatcher 的检测集合中被激活，可以通过调用 dispatch 函数获取这个文件描述符，并进一步找到对应的 Channel
- 回调函数与事件处理：一旦找到对应的 Channel ，可以根据触发的事件调用在 Channel 结构体中 注册的回调函数
- ChannelMap 结构体：用于存储 文件描述符 与 Channel 之间的对应关系。它是一个数组，下标（即 数组索引 ）对应文件描述符的值
- 扩容 ChannelMap ：如果当前 ChannelMap 的容量不足，可以通过调用 makeMapRoom 函数进行扩容

【C++实现Dispatcher】

在C++中Dispatcher被定义为一个抽象类（抽象接口），其定义了事件分发的相关控制流程（如添加、删除、事件监听等），因为不同IO复用方式细节实现上并不相同，所以功能函数采用虚函数定义，在运行时指向具体子类实例，根据实例类型自动执行相应方法

```c++
class Dispatcher
{
public:
    Dispatcher(EventLoop* evloop);
    //多态使用时，如果有子类中属性开辟在堆区，那么父类指针在释放时，无法调用到子类的析构代码
    virtual ~Dispatcher(); //虚析构函数，防止内存泄漏泄露
    // 添加
    virtual int add();
    // 删除
    virtual int remove();
    // 修改
    virtual int modify();
    // 事件监测
    virtual int dispatch(int timeout = 2); // 单位: s
    inline void setChannel(Channel* channel)
    {
        m_channel = channel;
    }
protected:
    string m_name = string();
    Channel* m_channel;
    EventLoop* m_evLoop;
};

【拓展】
虚函数是类中多态的一种，属于动态多态，提高了代码的拓展性
纯虚函数的目的是使派生类仅仅只继承函数的接口；抽象类的主要作用是将有关的操作作为结果接口组织在一个继承层次结构中，由它来为派生类提供一个公共的根，派生类将具体实现在其基类中作为接口的操作。
这就是动态多态的一种实现方式，EpollDispatcher、SlectDispatcher、PollDispatcher继承Dispatcher，并重写父类的虚函数
    
分类：静态多态（函数重载，运算符重载）；动态多态（派生类和虚函数实现运行时多态）
静态多态与动态多态的区别：
1.静态多态的函数地址早绑定（编译阶段确定函数地址）；
2.动态多态的函数地址晚绑定（运行阶段确定函数地址）； 
动态多态的满足条件：1、有继承关系；2、子类要重写父类的虚函数
动态多态的使用：父类的指针或引用指向子类对象

多态的优点：代码结构清晰；可读性强；对于前期以及后期扩展和维护性高
纯虚函数与抽象类
纯虚函数语法：virtual 返回值类型 函数名（参数列表）= 0；当类中有了纯虚函数，这个类就叫抽象类
抽象类特点：无法实例化对象；子类必须重写抽象类中的纯虚函数，否则也属于抽象类
虚析构与纯虚析构
多态使用时，如果有子类中属性开辟在堆区，那么父类指针在释放时，无法调用到子类的析构代码
解决办法：将父类中的析构函数改为虚析构或纯虚析构
虚析构与纯虚析构的共性：可以解决父类指针释放子类对象；都需要有具体的函数实现
区别是：如果是纯虚析构。该类属于抽象类，无法实例化对象
虚析构语法：virtual ~类名(){}
纯虚析构语法：virtual ~类名() = 0;（类内声明）	类名：：~类名(){} （类外实现）
```

### EventLoop模块

<img src="C:\Users\PC\Pictures\image\image-20250619100432044.png" alt="image-20250619100432044" style="zoom:80%;" />

```c
**************************EventLoop模型结构（上图对应的框架）***********************************
struct EventLoop{
    bool isQuit; //开关
    struct Dispatcher* dispatcher;
    void* dispatcherData;

    // 任务队列
    struct ChannelElement* head;
    struct ChannelElement* tail;

    // 用于存储channel的map
    struct ChannelMap* channelMap;

    // 线程ID，Name，mutex
    pthread_t threadID;
    char threadName[32];
    pthread_mutex_t mutex;
    int socketPair[2]; //存储本地通信的fd 通过socketpair初始化
};
```

Dispatcher是一个事件分发模型，通过这个模型,就能够检测对应的文件描述符的事件的时候,可以使用epoll/poll/select,前面说过三选一。另外不管是哪一个底层的检测模型,它们都需要使用一个数据块,这个数据块就叫做DispatcherData。除此之外,还有另外一个部分,因为在这个反应堆模型里边对应一系列的文件描述符,都属于epoll/poll/select,但是这些文件描述符,它们不是固定的，可能可客户端建立连接，那么就需要把某个文件描述符添加到Dispatcher中，或者客户端要断开连接，那么就需要从对应的检测集中删除掉，或者是要对该文件描述符进行修改。基于上述情况，可以把这些都称作任务，既然是任务，如果产生多个这样的任务，就需要把这些任务存储起来——**任务队列**（c++中可以使用STL直接使用）

<img src="C:\Users\PC\Pictures\image\image-20250619154524732.png" alt="image-20250619154524732" style="zoom:67%;" />

```c
//定义任务队列节点
struct ChannelElement{
    int type; //如何处理节点中的channel
    struct Channel* channel;
    struct ChannelElement* next;
};

// 处理该节点中的channel的方式
enum ElemType{
    ADD,    // 添加
    DELETE, // 删除
    MODIFY  // 修改
};

既然它是一个任务队列,也就意味着这个队列里边的节点的个数是不固定的,所以我们就需要一个动态的模型,可以实现一个链表。这个链表的节点是什么类型的?是ChannelElement类型.所谓ChannelElement类型。它里边主要其实是一个Channel，还有下一个节点的指针。通过指向下一个节点的指针，就可以把每一个节点连接起来了。当这个任务队列里边有了任务之后，我们就需要通过一个循环，把链表里边所有的节点都读出来。之后通过检测节点中的事件（添加、删除、修改）来对Dispatcher检测集合做修改
```

EventLoop里边有一个任务队列，可以说这个EventLoop它是一个生产者和消费者模型。

- 消费者——Dispatcher；
- 生产者——生产者有可能是其他的线程，比如说主线程和客户端建立了连接，剩下的事就是通信。如果要通信，就对应一个通信的文件描述符。主线程就把这个任务添加到了子线程对应的这个EventLoop里边。此时在taskQueue里边就多出来了一个节点。在遍历这个任务队列的时候，读到这个节点之后，就需要把当前的这个节点添加到Dispatcher对应的检测模型里边

另外，在这个EventLoop里边，还有一个ChannelMap，这个ChannelMap也是我们实现的，是通过一个数组来实现的。基于这个ChannelMap，就能够通过一个文件描述符得到对应的那个channel，为什么要得到那个channel呢？因为在这个channel里边有文件描述符，它的读事件和写事件对应的回调函数，就是事件触发之后，执行什么样的处理动作

【思考】ChannelMap的使用：

在实现epoll/select/poll的时候，分别调用了epoll_wait函数/select函数/poll函数，通过遍历内核传出来的这个集合，就得到了触发对应事件的那个文件描述符。但我们现在处理不了，因为得不到对应的channel,我们可以通过EventLoop里边提供的这个ChannelMap就能够得到对应的Channel指针，这个Channel指针就可以调用事件对应的处理函数了

这三大块之外，还有一些其他的数据成员，比如threadID。因为在当前的服务器里边有多个EventLoop，每个EventLoop都属于一个线程。所以我们可以记录一下这个EventLoop它所对应的那个子线程的线程ID。关于子线程的这个名字肯定是我们给它起的，因为子线程创建出来之后，它只有一个ID，这是系统分配的。关于它的名字，操作系统是没有告诉我们的

【思考】互斥锁保护的是什么？

其实它保护的是这个任务队列。为什么要保护任务队列呢？对于这个EventLoop来说，它能够被多少个线程操作呢？

- 如果是主线程的EventLoop，那就是一个。

- 如果是子线程的EventLoop，那就有可能是两个，为什么是两个呢？

  - 当前，线程在执行这个EventLoop的时候，它肯定要遍历这个taskQueue吧，也就是当前线程需要读这个任务队列。

  - 除此之外，如果主线程和客户端建立了一个连接,主线程是有可能要把一个任务添加到这个EventLoop,对应的任务队列里边，就是额外的另一个线程了。

  - 如果涉及到两个线程操作，同一块共享资源，那么我们是要通过互斥锁来保护这个共享资源的。如果不保护，肯定就会出现数据混乱。

<img src="C:\Users\PC\Pictures\image\image-20250619161319490.png" alt="image-20250619161319490" style="zoom:100%;" />

整个项目的结构，在当前这个多反应堆模型的服务器程序里边，它是有多个EventLoop模型的。首先，在主线程里边就有一个EventLoop主线程的，这个EventLoop去检测客户端有没有新的连接到达。如果有新连接，就建立新连接。之后，主线程把这个通信的任务给到线程池里边，把主线程的那个EventLoop也传进来。一定要注意这个EventLoop和主线程的EventLoop是同一个实例。也就是说，线程池里边的这个MainEventLoop和外边这个MainEventLoop它们对应的是同一块内存地址.另外，在线程池里边还有若干个子线程，每个子线程里边都对应一个EventLoop。每个子线程里边的EventLoop它们主要是处理通信的文件描述符相关的操作。这些都是在子线程里边来完成的

【思考】那么，为什么右侧的TcpConnection里边也有一个EventLoop呢？

关于这个TcpConnection，其实它是封装了用于通信的文件描述符，在这个模块里边，是把子线程里边那个EventLoop的地址传递给了TcpConnection。
在每个线程里边，都有一个EventLoop,也就是说EventLoop是属于线程的，不管是主线程还是子线程，里边都有一个EventLoop。然后在这个TcpConnection里边，也有一个EventLoop，但是不是说EventLoop属于TcpConnection，而是TcpConnection属于EventLoop。
如果TcpConnection，它属于EventLoop，那么这个TcpConnection就属于对应的某一个子线程。EventLoop属于哪个子线程，这个TcpConnection它就属于哪个子线程。它对应的那些任务处理就是和客户端通信，接收数据以及发送数据的操作就在哪个子线程里边来完成。在线程池里边传进来了一个主线程EventLoop，主线程的EventLoop也是一个反应堆实例

【思考】为什么要把主线程的反应堆实例传递给线程池呢？

是因为我们在给线程池做初始化的时候，如果指定线程池的子线程个数为0，此时线程池就没有了，不能工作。为了能够保证线程池能够工作，也就传进来了一个主线程的反应堆实例，在没有子线程的情况下，那么就借用主线程的反应堆实例来完成对应的这一系列的任务处理。在此时，客户端和服务器建立连接之后，得到了用于通信的文件描述符，这个通信的文件描述符被TcpConnection封装起来了。我们就需要把这个TcpConnection放到一个反应堆模型里边，就是放到主线程的EventLoop里边，这样客户端和服务器的通信操作也就能实现了。这种比较极端的情况，对于程序来说，它就是一个单反应堆模型。

如果在创建线程池的时候指定这个子线程的个数大于0，那么就是一个多反应堆的服务器模型。
如果在创建线程池的时候指定线程的个数等于0，此时就是一个单反应堆的服务器模型

上述的整体处理流程如下：

- 当反应堆启动后，检测被激活的文件描述符
- 调用dispatcher函数得到文件描述符fd
- 根据fd从channelMap中取出对应的channel，判断读事件还是写事件
- 调用对应的回调函数处理事件

sockpair函数

```c
#include <sys/socket.h>
int socketpair(int domain, int type, int protocol, int sv[2]);
```

Linux 中的 socketpair 函数用于创建一个双向的 socket 连接, 通常用于父子进程之间的通信 。在上述代码中, socketpair 函数用于初始化一个事件循环( EventLoop )对象,并为其分配一个 socket pair ,用于接收来自子进程的数据

<img src="C:\Users\PC\Pictures\image\image-20250620180043447.png" alt="image-20250620180043447" style="zoom:80%;" />

关于任务队列处理的两个路径：

- 第一个路径： 子线程往任务队列里边添加一个任务，比如说修改文件描述符里边的事件，肯定是子线程自己修改自己检测的文件描述符的事件，修改完了之后，子线程就直接调用这个函数去处理任务队列里边的任务。
- 第二个路径： 主线程在子线程的任务队列里边添加了一个任务，主线程是处理不了的，并且主线程现在也不知道子线程是在工作还是在阻塞，所以 主线程就默认子线程现在正在阻塞 ，因此主线程就调用了一个唤醒函数（ taskWakeup ）来解除阻塞，调用这个函数保证子线程肯定是在运行的， 而 子线程 是 eventLoopRun函数 的 dispatch函数 的 调用位置解除了阻塞 ，然后调用 eventLoopProcessTask(evLoop);

【c++实现EventLoop模块】

```c++
class EventLoop
{
public:
    EventLoop();
    EventLoop(const string threadName);
    ~EventLoop();
    // 启动反应堆模型
    int run();
    // 处理别激活的文件fd
    int eventActive(int fd, int event);
    // 添加任务到任务队列
    int addTask(struct Channel* channel, ElemType type);
    // 处理任务队列中的任务
    int processTaskQ();
    // 处理dispatcher中的节点
    int add(Channel* channel);
    int remove(Channel* channel);
    int modify(Channel* channel);
    // 释放channel
    int freeChannel(Channel* channel);
    int readMessage();
    // 返回线程ID
    inline thread::id getThreadID()
    {
        return m_threadID;
    }
    inline string getThreadName()
    {
        return m_threadName;
    }
    static int readLocalMessage(void* arg);

private:
    void taskWakeup();

private:
    bool m_isQuit;
    // 该指针指向子类的实例 epoll, poll, select
    Dispatcher* m_dispatcher;
    // 任务队列
    queue<ChannelElement*> m_taskQ;
    // map
    map<int, Channel*> m_channelMap;
    // 线程id, name, mutex
    thread::id m_threadID;
    string m_threadName;
    mutex m_mutex;
    int m_socketPair[2];  // 存储本地通信的fd 通过socketpair 初始化
};

注：
    1.channelMap采用哈希表来构建
    2.任务队列直接使用STL的queue，无需自己构建任务队列
```

### ThreadPool与WorkerThread

#### WorkerThread

<img src="C:\Users\PC\Pictures\image\image-20250620214910256.png" alt="image-20250620214910256" style="zoom:67%;" />

```c
// 定义子线程对应的结构体
struct WokerThread {
    pthread_t threadID;// 线程ID
    char name[24];// 线程名字
    pthread_mutex_t mutex;// 互斥锁（线程同步）
    pthread_cond_t cond;// 条件变量（线程阻塞）
    struct EventLoop* evLoop;// 事件循环(反应堆模型)
    // 在每个子线程里边都有一个反应堆模型
};
```

#### ThreadPool

![image-20250620215622227](C:\Users\PC\Pictures\image\image-20250620215622227.png)

```c
// 定义线程池
struct ThreadPool {
    /*
        在线程池里边的这个mainLoop主要是做备份用的，主线程里边的mainLoop主要负责和
        客户端建立连接，只负责这一件事情，只有当线程池没有子线程这种情况下，mainLoop
        才负责处理和客户端的连接，否则的话它是不管其他事情的。
    */
    struct EventLoop* mainLoop; // 主线程的反应堆模型

    int index; 
    bool isStart;
    int threadNum; // 子线程总个数
    struct WorkerThread* workerThreads;
};
```

- 在线程池里边其实管理一个 WorkerThreads 数组，在这个数组里边有若干个元素，每个元素里边都有一个 WorkerThread 对象，把这若干个子线程对象初始化出来。之后，每个子线程里边都有一个 EventLoop （反应堆模型），子线程处理的任务其实就是 EventLoop 里边的任务。
- 除此之外，在线程池里边还可以添加一个变量用来标记当前线程池是否启动： isStart 默认情况下，肯定是不启动的状态的，即 isStart = false;
- ThreadNum 用来记录当前线程池里边的子线程的个数，就是 WorkerThreads 数组的元素个数，它们有对应关系。
- index 记录线程池里边子线程的编号，通过这个 index 就能够访问线程池里边某个线程。

【思考】：为什么我们要访问线程池里边的某一个线程？

场景：主线程和客户端建立了连接，之后，就需要和客户端通信，那么 这个通信的文件描述符和客户端通信流程需要交给子线程去处理 ，每个子线程里边都有一个 EventLoop (反应堆模型)，就需要把这个通信的文件描述符交给这个反应堆模型去管理， 检测这个文件描述符的事件，如果有读事件，说明客户端有数据发送过来，那么我们就需要接收数据，然后再给客户端回复数据，这些都是在子线程的反应堆模型里边做的。 所以 当这个连接建立之后，主线程就要从线程池里边找出一个子线程，并且把这个任务给到子线程去处理。
但是存在一个问题：假设只有 三个 子线程，在找子线程的时候，不可能每次都找 workerThread1 ,如果每次都是 workerThread1 ,那么 workerThread2 , workerThread3 就空闲下来了，为了雨露均沾， 可以用 index 来 表示当前访问的线程到底是谁 ，如果这次访问线程1，那么下次就是线程2。如果这次访问线程2，那么下次就是线程3。如果这次访问线程3，那么下次就是线程1。

子线程反应堆实例的取出

- 可以通过 takeWorkerEventLoop 函数从线程池中取出子线程的反应堆实例
- 这个函数的核心是取出反应堆实例，用于处理任务
- 如果线程数量为零，evLoop 为主线程的反应堆模型( mainLoop )
- 对线程池中的工作线程实现雨露均沾地分配各个子线程调度，避免所有任务都由同一个线程处理，还确保index在合适的取值范围

在启动线程池时，需要传入有效指针， 确保线程池未被运行，并判断执行线程的线程是否为主线程。 通过 WorkerThread 模块中的函数对子线程进行初始化并启动。主线程可以方便地管理子线程，并从中选择一个子线程以获取其反应堆模型。此外，线程池操作的函数包括初始化、启动和取出子线程等。 整个处理流程需要确保每个任务都能被雨露均沾地分配给各个子线程，避免所有任务都由同一个线程处理，还确保了index在合适的取值范围

![image-20250621141026372](C:\Users\PC\Pictures\image\image-20250621141026372.png)

### Buffer模块

TcpConnection:封装的就是建立连接之后得到的用于通信的文件描述符，然后基于这个文件描述符，在发送数据的时候，需要把数据先写入到一块内存里边，然后再把这块内存里边的数据发送给客户端，除了发送数据，剩下的就是接收数据。接收数据，把收到的数据先存储到一块内存里边。也就意味着，无论是发送数据还是接收数据，都需要一块内存。并且这块内存是需要使用者自己去创建的。所以就可以把这块内存做封装成Buffer

<img src="C:\Users\PC\Pictures\image\image-20250621142456093.png" alt="image-20250621142456093" style="zoom:50%;" />

```c
struct Buffer
{
    // 指向内存的指针
    char* data;
    int capacity;
    int readPos;
    int writePos;
};
```

关于buffer的几个注意点

- 在实现 bufferAppendData 函数时，需要考虑如何处理内存的写入和接收数据的情况。在写数据之前，可能需要进行内存扩容以确保有足够的空间。写数据时，需要从上次写入的 writePos 位置开始。完成写入后，需要再次更新 writePos 的位置
- 写内存的方式：直接写入：将数据存储到 buf 结构体对应的内存空间；基于套接字接收数据：使用 readv 等函数；

### TcpConnection模块

对于一个TcpServer来说，它的灵魂是什么？就是需要提供一个事件循环EventLop(EventLoop)，不停地去检测有没有客户端的连接到达，有没有客户端给服务器发送数据，描述的这些动作，反应堆模型能够胜任。当服务器和客户端建立连接之后，剩下的就是网络通信，在通信的时候，需要把接收的数据和要发送的数据存储到一块内存里边，Buffer(Buffer)就是为此量身定制的。另外，如果服务器想和客户端实现并发操作，需要用到多线程。我们提供了线程池ThreadPool(ThreadPool),剩下的事就是把服务器模型里边的代码实现一下，基于服务器的整体流程实现一下TcpConnection。

这个TcpConnection就是服务器和客户端建立连接之后，它们是在通信的时候，其实就需要用到Http,如果想要实现一个HttpServer，就需要用到Http协议，再把HttpRequest和HttpResponse写出来之后，整个项目的流程就全部走通了。

**服务器和客户端建立连接和通信流程**

于多反应堆模型的服务器结构图，这主要是一个 TcpServer ，关于 HttpServer ,主要是用了 Http 协议，核心模块是 TcpServer 。这里边有两种线程：主线程和子线程。子线程是在线程池里边，线程池的每个子线程都有一个反应堆模型，每个反应堆模型都需要有一个 TcpConnection 。

如果这个反应堆实例所属的线程是主线程，主线程是如何在这个反应堆模型里边工作的呢？在服务器端有一个用于监听的文件描述符 ListenFd (简写为 lfd )，基于 lfd 就可以和客户端建立连接，如果想要让 lfd 去工作，就得把它放到反应堆模型里边，首先要对 lfd 封装成 Channel 类型，之后添加到 TaskQueue 这个任务队列里边，接着 MainEventLoop 就会遍历 TaskQueue ，取出对应的任务节点（ ChannelElement ），基于任务节点里边的 type 对这个节点进行添加/删除/修改操作。

补充说明 ：取出这个节点之后，判断这个节点的类型 type ,如果 type==ADD ,把 channel 里边的文件描述符 fd 添加到 Dispatcher 的检测集合中；如果t ype==DELETE, 把 channel 里边的文件描述符 fd 从 Dispatcher 的检测集合中删除；如果 type==MODIFY ,把 channel 里边的文件描述符 fd 在 Dispatcher 的检测集合中的事件进行修改。主线程往属于自己的反应堆模型里边放的文件描述符是用于监听的，那么这个 lfd 肯定是要添加到 Dispatcher 的检测集合里边，所以操作肯定是添加操作（ ADD ）。

很显然，这个 lfd 需要 添加 到反应堆模型的 Dispatcher 里边， Dispatcher 主要封装了 poll/epoll/select 模型，不管使用了这三个里边的哪一个，其实都需要对用于监听的文件描述符的读事件进行检测。在检测的时候，如果是 epoll 模型，它会调用 epoll_wait 函数; 如果是 poll 模型，它会调用 poll 函数;如果是 select 模型，它会调用 select 函数;通过这三个函数，传出的数据，我们就能够知道用于监听的文件描述符lfd它对应的读事件触发了。对应的读事件触发了，就可以基于得到的文件描述符（此处为 lfd ）。通过 ChannelMap 里边的 fd （ fd 其实就是数组的下标）可以找到对应的 channel 地址，那么基于 lfd 就可以找到对应的 channel 地址，就能知道lfd所对应的读事件要干什么。也就是和客户端建立连接，也就可以得到一个通信的文件描述符（ cfd ）。

首先把用于通信的文件描述符封装成一个 Channel 类型，接着把 channel 封装到 TcpConnection 模块里边。另外，这个 TcpConnection 模块需要在子线程里边运行的，故需要通过子线程去访问线程池，从线程池找出一个子线程，每个子线程都有一个 EventLoop ,再把子线程的 EventLoop 也放到我们封装的 TcpConnection 模块里边。也就是把子线程的反应堆实例传给 TcpConnection 模块。

一定要注意： TcpConnection 模块里边的 EventLoop 是属于子线程的，是从子线程传过来的一个反应堆模型的地址。然后就可以在 TcpConnection 模块里边通过 Channel 里边封装的通信的文件描述符( cfd )和客户端进行通信，就是接收数据和发送数据。关于通信的文件描述符的事件检测，读事件或者是写事件检测都是通过 EventLoop 来实现的。

<img src="C:\Users\PC\Pictures\image\image-20250621152302565.png" alt="image-20250621152302565" style="zoom:67%;" />

```c
struct TcpConnection
{
    struct EventLoop* evLoop;
    struct Channel* channel;
    struct Buffer* readBuf;
    struct Buffer* writeBuf;
    char  name[32];
    // http协议
    struct HttpRequest* request;
    struct HttpResponse* response;
};
```

每个通信的文件描述符都对应一个 TcpConnection ,并且每个 TcpConnection 都对应一个子线程。假设说我现在有 10 个 TcpConnection , 4 个线程，那么每个通信的文件描述符所对应的 TcpConnection 的 Name 是不一样的。但是，有可能有若干个 TcpConnection 是在同一个子线程里边执行的。在处理任务时，进行套接字通信的线程个数是有限的。并且，所以不同的 TcpConnection 有可能是在同一个线程里边被处理的，但是每个 TcpConnection 里边都有一个用于通信的文件描述符，这个文件描述符对应的 连接的名字（Name） 是唯一的。如果你发现出现相同的名字的，除非是这个文件描述符通信完了之后被释放了，而我们又建立了新的连接。被释放的这个文件描述符被复用了，所以我们就会发现当前的这个文件描述符对应的连接的名字和之前的某个文件描述符对应的连接的名字是相同的。

在初始化过程中，把用于通信的文件描述符 cfd 作为参数传入 tcpConnectionInit 里去，也就是 fd 为用于通信的文件描述符。将 fd 封装成 channel 。需要检测文件描述符什么事件呢?在服务器端通过文件描述符 fd 和客户端通信，如果客户端不给服务器发数据，服务器就不会给客户端回数据。因此在服务器端迫切想知道的有没有数据到达：就是有没有 发过来请求数据 。关于这个 读事件 我们需要指定一个 processRead 回调函数。若接收到数据，之后把 channel 添加到事件循环对应的任务队列里边去。

当文件描述符的 读事件触发 时，表示有客户端发送了数据。在通信的文件描述符内核对应的 读缓冲区 里边已经有数据了，我们就需要 把数据从内核 读到 自定义的Buffer 实例里边，就是 conn （ TcpConnection 实例）里边的 readBuf 。故需要给这个 processRead 回调函数传递的 实参 是 conn （ TcpConnection 实例）。因为在 conn 里边，既有需要的 readBuf ，也有文件描述符 fd 。这个 fd 就是通信的文件描述符。它已经被封装到了这个 channel 里边。在 processRead 回调函数里边，先对参数 arg 进行类型转换。然后我们就可以接收数据了。接收到的数据最终要存储到 readBuf 里边。 readBuf 对应的是一个 Buffer 结构体，在这个 Buffer 结构体里边，我们提供了一个读取套接字数据的 bufferSocketRead 函数，我们只需要把 readBuf （ Buffer 实例）传进来，也把文件描述符传进 bufferSocketRead 函数。那么接收到的数据就存储到了这个 readBuf 结构体对应的那块内存里边。

### TcpServer模块

![image-20250621160333755](C:\Users\PC\Pictures\image\image-20250621160333755.png)

```c
struct Listener {
    int lfd;
    unsigned short port;
};
 
struct TcpServer {
    struct Listener* listener; // 监听套接字
    struct EventLoop* mainLoop; // 主线程的事件循环（反应堆模型）
    struct ThreadPool* threadPool; // 线程池
    int threadNum; // 线程数量
};
```

TcpConnection其实是子线程的任务。服务器与客户端建立连接之后，子线程要是想工作的话，就必须创建一个TcpConnection实例。如果没有这个TcpConnection模块，子线程就不知道在建立连接之后需要做什么事情

**TCP服务器初始化与线程池、事件循环的启动**

- 线程池的创建与启动：在初始化TcpServer时，线程池随之被创建。 启动线程池的函数是threadPoolRun函数
- 事件循环的启动与任务添加：服务器启动后，需要将监听的文件描述符添加到事件循环（mainLoop反应堆）中。 添加任务的函数是eventLoopAddTask函数
- 知识点3：Channel的初始化与使用：获取channel实例需要调用初始化函数channelInit函数。 当读事件触发时，表示有新的客户端连接到达，需要与其建立连接。

### Http模块

#### httprequest

```c
// 当前的解析状态
enum HttpRequestState {
    ParseReqLine,    // 当前解析的是请求行
    ParseReqHeaders, // 当前解析的是请求头
    ParseReqBody,    // 当前解析的是请求体
    ParseReqDone     // 完成解析
};
 
// 定义 http 请求结构体
struct HttpRequest {
    // 当前解析状态
    enum HttpRequestState curState;  
    // 请求行
    char* method;
    char* url;
    char* version;  
    // 请求头
    struct RequestHeader* reqHeaders;
    int reqHeadersNum;
};
```

当一个浏览器和服务器建立连接之后，浏览器给服务器发送数据，服务器接收到浏览器的请求消息之后，就需要进行解析了。解析之后的数据存储到 HttpRequest 结构体里边，对于 method,url,version 它们是指针，其实是需要指向一块有效的内存的。再给这个指针赋值的时候，需要先 malloc 一块内存，再让指针指向这块内存。如果这个浏览器和服务器建立连接之后，它们进行了多次通信，那么就需要多次往 HttpRequest 结构体里边写数据，第一次写的时候， method 对应一块堆内存。那么再次写入，需要把上次写入的数据清除掉，由于清除的是堆内存，可以用 free 函数 （防止内存泄漏）

（1）关于指针与内存

- 在给指针赋值之前，需要先分配一块有效的内存
- 当浏览器客户端与服务器建立连接并进行多次通信时，需要多次向 HttpRequest结构体 中写入数据
- 如果需要重置 HttpRequest 结构体，只需重置指针指向的内存中的数据，而不需要 销毁整个结构体的内存地址
- 当重置指针时， 如果之前没有释放对应的内存，会导致内存泄漏

（2）关于释放内存

- 在释放内存时， 应先释放结构体成员中的指针所指向的内存
- 对于指向请求行数据的 url、method和version ，可以直接调用 HTTP 请求重置函数进行释放
- 对于结构体中的 reqHeaders 指针指向的内存，需要先判断指针是否为空，再释放对应的内存

#### httpresponse

```c
// 定义状态码枚举
enum HttpStatusCode {
    Unknown = 0,
    OK = 200,
    MovedPermanently = 301,
    MovedTemporarily = 302,
    BadRequest = 400,
    NotFound = 404
};
 
// 定义响应的结构体
struct ResponseHeader {
    char key[32];
    char value[128];
};
 
// 定义一个函数指针，用来组织要回复给客户端的数据块
typedef void (*responseBody) (const char* fileName,struct Buffer* sendBuf,int socket);
 
// 定义结构体
struct HttpResponse {
    // 状态行:状态码，状态描述
    enum HttpStatusCode statusCode;
    char statusMsg[128];
    // 响应头 - 键值对
    struct ResponseHeader* headers;
    int headerNum;
    /*
        服务器回复给客户端的数据取决于客户端向服务器请求了什么类型的资源,
        有可能它请求的是一个目录,有可能请求的是一个文件,这个文件有可能是
        一个文本文件,也可能是一个图片,还可能是mp3...需要根据客户端的请求
        去回复相应的数据.所以如何去组织这个需要回复的数据块呢?
        - 定义一个函数指针，用来组织要回复给客户端的数据块
        fileName:分成两类,一类是目录类型,一类是非目录类型的文件
        如果是目录就去遍历目录,如果是文件,就读取其内容
        在进行套接字的通信过程中,如果要接收数据,它就是用来存储客户端
        发过来的数据块;
        如果要回复数据(给客户端发数据),发送的数据要先存储到sendBuf
        里边,再发送给客户端.
        socket:就是用来通信的文件描述符
        通过这个用于通信的文件描述符,就能够把写入到sendBuf里边的数据发送给客户端.
        sendBuf里边的数据就是我们组织好的Http响应的数据块
    */
    responseBody sendDataFunc;
    // 文件名
    char fileName[128];
};
```

服务器回复给客户端的数据，取决于客户端向服务器请求了什么类型的资源,有可能 它请求的是一个目录,有可能请求的是一个文件 ,这个文件有可能是一个文本文件,也可能是一个图片,还可能是 mp3 ...需要根据客户端的请求去回复相应的数据。所以如何去组织这个需要回复的数据块呢?

```c
// 定义一个函数指针，用来组织要回复给客户端的数据块
typedef void (*responseBody) (const char* fileName,struct Buffer* sendBuf,int socket);
```

fileName :分成 两类 , 一类是目录类型, 一 类是非目录类型的文件。如果是 目录， 就去遍历 目录；如果是 文件 ，就 读取其内容

sendBuf :在进行套接字的通信过程中：如果要 回复数据 (给客户端发数据),发送的数据要先存储到 sendBuf 里边,再发送给客户端.

socket :就是用来通信的文件描述符，通过这个用于通信的文件描述符,就能够把写入到 sendBuf 里边的数据发送给客户端， sendBuf 里边的数据就是我们组织好的 Http 响应的数据块

定义一个函数指针，用 来组织要回复给客户端的数据块：responseBody sendDataFunc;