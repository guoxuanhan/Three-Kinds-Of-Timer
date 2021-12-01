/*
    时间堆定时器的设计思路：将所有定时器中超时时间最小的一个定时器的超时值作为心搏间隔。一旦心搏函数tick被调用，
    超时时间最小的定时器必然到期，我们就可以在tick函数中处理该定时器。然后在从剩余的定时器中找出超时时间最小的一个，
    并将这段最小时间设置为下一次心搏间隔，如此反复就实现了较为精准的定时。

    时间堆定时器的数据结构和算法采用了最小堆，它是一个完全二叉树。

    从执行效率来看：
        添加一个定时器的时间复杂度O(logn)
        删除一个定时器的时间复杂度O(1)
        执行一个定时器的时间复杂度O(1)
*/

#ifndef TIME_HEAP_TIMER_HPP
#define TIME_HEAP_TIMER_HPP

#include <iostream>
#include <netinet/in.h>
#include <time.h>
using std::exception;

#define BUFFER_SIZE 64
class heap_timer;

// 用户数据结构
struct client_data
{
    sockaddr_in address;    // 客户端socket地址
    int sockfd;             // socket文件描述符
    char buf[BUFFER_SIZE];  // 读缓存
    heap_timer* timer;      // 定时器
};

// 定时器类
class heap_timer
{
public:
    heap_timer(int delay)
    {
        expire = time(NULL) + delay;
    }
public:
    time_t expire;                  // 定时器生效的绝对时间
    void (*cb_func)(client_data*);  // 定时器回调函数
    client_data* user_data;         // 用户数据
};

// 时间堆类
class time_heap
{
public:
    // 构造函数之一：初始化一个大小为cap的空堆
    time_heap(int cap) throw(std::exception) : capacity(cap), cur_size(0)
    {
        // 创建堆数组
        array = new heap_timer*[capacity];
        if(!array)
        {
            throw std::exception();
        }
        for(int i = 0; i < capacity; ++i)
        {
            array[i] = nullptr;
        }
    }

    // 构造函数之二：用已有的数组来初始化堆
    time_heap(heap_timer** init_array, int size, int capacity) throw(std::exception) : 
        capacity(capacity), cur_size(size)
    {
        if(capacity < size)
        {
            throw std::exception();
        }
        // 创建堆数组
        array = new heap_timer*[capacity];
        if(!array)
        {
            throw std::exception();
        }
        for(int i = 0; i < capacity; ++i)
        {
            array[i] = nullptr;
        }
        if(size != 0)
        {
            // 初始化堆数组
            for(int i = 0; i < size; ++i)
            {
                array[i] = init_array[i];
            }
            for(int i = (cur_size-1)/2; i >= 0; i--)
            {
                // 对数组中的第[(cur_size-1)/2]~0个元素进行下虑操作
                percolate_down(i);
            }
        }
    }

    // 销毁时间堆
    ~time_heap()
    {
        for(int i = 0; i < cur_size; ++i)
        {
            delete array[i];
        }
        delete []array;
    }
public:
    // 添加目标定时器
    void add_timer(heap_timer* timer) throw(std::exception)
    {
        if(!timer)
        {
            throw std::exception();
        }
        // 如果当前堆数组容量不够，扩大一倍容量
        if(cur_size >= capacity)
        {
            resize();
        }
        // 新插入了一个元素，当前堆的大小加1，hole是新建空节点的位置
        int hole = cur_size++;
        int parent = 0;
        // 对从空节点到根节点的路径上的所有节点执行上虑操作
        for(; hole > 0; hole = parent)
        {
            // 计算父节点的位置
            parent = (hole-1)/2;
            if(array[parent]->expire <= timer->expire)
            {
                break;
            }
            array[hole] = array[parent];
        }
        array[hole] = timer;
    }

    // 删除目标定时器timer
    void del_timer(heap_timer* timer)
    {
        if(!timer)
        {
            return;
        }
        // 仅仅将目标定时器的回调函数设置为空，即所谓的延迟销毁。这将节省真正删除该定时器
        // 造成的开销，但这样做也容易使堆数组膨胀
        timer->cb_func = nullptr;
    }

    // 获得堆顶部的定时器
    heap_timer* top() const
    {
        if(empty())
        {
            return nullptr;
        }
        return array[0];
    }

    // 删除堆顶部的定时器
    void pop_timer()
    {
        if(empty())
        {
            return;
        }
        if(array[0])
        {
            delete array[0];
            // 将原来的堆顶元素替换为堆数组中最后一个元素
            array[0] = array[--cur_size];
            // 对堆数组执行下虑操作
            percolate_down(0);
        }
    }

    // 心搏函数
    void tick()
    {
        heap_timer* tmp = array[0];
        time_t cur = time(NULL);
        // 循环处理堆数组中到期的定时器
        while(!empty())
        {
            if(!tmp)
            {
                break;
            }
            // 如果堆顶定时器没有到期，则退出循环
            if(tmp->expire > cur)
            {
                break;
            }
            // 否则就执行堆顶定时器中的任务
            if(array[0]->cb_func)
            {
                array[0]->cb_func(array[0]->user_data);
            }
            // 删除堆顶定时器，同时生成新的堆顶定时器
            pop_timer();
            tmp = array[0];
        }
    }

    // 堆数组是否为空
    bool empty() const
    {
        return 0 == cur_size;
    }
private:
    // 最小堆的下虑操作，它确保堆数组中以第hole个节点作为根的子树拥有最小堆的性质
    void percolate_down(int hole)
    {
        heap_timer* temp = array[hole];
        int child = 0;
        for(; ((hole*2+1) <= cur_size - 1); hole = child)
        {
            child = hole*2+1;
            if(child < (cur_size - 1) && array[child + 1]->expire < array[child]->expire)
            {
                ++child;
            }
            if(array[child]->expire < temp->expire)
            {
                array[hole] = array[child];
            }
            else
            {
                break;
            }
        }
        array[hole] = temp;
    }

    // 将堆数组容量扩大一倍
    void resize() throw(std::exception)
    {
        heap_timer** temp = new heap_timer*[2*capacity];
        for(int i = 0; i < 2*capacity; ++i)
        {
            temp[i] = nullptr;
        }
        if(!temp)
        {
            throw std::exception();
        }
        capacity = 2*capacity;
        for(int i = 0; i < cur_size; ++i)
        {
            temp[i] = array[i];
        }
        delete []array;
        array = temp;
    }
private:
    heap_timer** array; // 堆数组
    int capacity;       // 堆数组的容量
    int cur_size;       // 对数组当前包含元素的个数
};

#endif