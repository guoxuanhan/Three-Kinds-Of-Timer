/*
    这是一个升序链表定时器示例，其核心函数tick相当于一个心搏函数，它每隔一段固定的时间就执行一次，以检测并处理
    到期的定时器任务。判断定时任务到期的依据是定时器的expire值小于当前的系统时间。
    
    从执行效率来看：
        添加定时器的时间复杂度是O(n)    添加效率偏低，后面的时间轮解决了这个问题
        删除定时器的时间复杂度是O(1)
        执行定时器任务的时间复杂度是O(1)
*/

#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>
#define BUFFER_SIZE 64

class util_timer;

// 用户数据结构
struct client_data
{
    sockaddr_in address;    // 客户端socket地址
    int sockfd;             // socket文件描述符
    char buf[BUFFER_SIZE];  // 读缓存
    util_timer* timer;      // 定时器
};

// 定时器类
class util_timer
{
public:
    util_timer() : prev(nullptr), next(nullptr) {}
public:
    time_t expire;                 // 任务的超时时间，这里用绝对时间
    void (*cb_func)(client_data*); // 任务回调函数
    client_data* user_data;        // 回调函数处理的客户数据，由定时器的执行者传递给回调函数
    util_timer* prev;              // 指向前一个定时器
    util_timer* next;              // 指向下一个定时器
};

// 定时器链表：它是一个升序、双向链表，且带有头结点和尾节点
class sort_timer_lst
{
public:
    sort_timer_lst() : head(nullptr), tail(nullptr) {}
    
    // 链表被销毁时，删除其中所有的定时器
    ~sort_timer_lst()
    {
        util_timer* tmp = head;
        while(tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    // 将目标定时器timer添加到链表中
    void add_timer(util_timer* timer)
    {
        if(!timer)
        {
            return;
        }
        if(!head)
        {
            head = tail = timer;
            return;
        }
        // 如果目标定时器的超时时间小于当前链表中的所有定时器的超时时间，则把该定时器插入链表头部，
        // 作为链表新的头结点。否则就需要条用重载函数add_timer把它插入链表中合适的位置，以保证链表
        // 的升序特性
        if(timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }

    // 当某个定时任务发生变化时，调整对应的定时器在链表中的位置。这个函数只考虑被调整的定时器的超时
    // 时间延长的情况，即该定时器需要往链表的尾部移动
    void adjust_timer(util_timer* timer)
    {
        if(!timer)
        {
            return;
        }
        util_timer* tmp = timer->next;
        // 如果被调整的定时器处于链表尾部，或者该定时器的超时值任然小于其他定时器超时值，则不调整
        if(!tmp || timer->expire < tmp->expire)
        {
            return;
        }
        // 如果目标定时器是链表头结点，则将该定时器从链表中取出并重新插入链表
        if(timer == head)
        {
            head = head->next;
            head->prev = nullptr;
            timer->next = nullptr;
            add_timer(timer, head);
        }
        // 如果目标定时器不是链表的头结点，则将该定时器从链表中取出，然后插入其原来所在位置之后的部分链表中
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    // 将目标定时器从链表中删除
    void del_timer(util_timer* timer)
    {
        if(!timer)
        {
            return;
        }
        // 下面这个条件成立表示链表中只有一个定时器，即目标定时器
        if(timer == head && timer == tail)
        {
            delete timer;
            head = tail = nullptr;
            return;
        }
        // 如果链表中至少有两个定时器，且目标定时器是链表头结点，则将链表的头结点后移，并删除目标定时器
        if(timer == head)
        {
            head = head->next;
            head->prev = nullptr;
            delete timer;
            return;
        }
        // 如果链表中至少有两个定时器，且目标定时器是链表为节点，则将链表的尾节点重置为原节点的前一个节点
        // ，然后删除目标定时器
        if(timer == tail)
        {
            tail = tail->prev;
            tail->next = nullptr;
            delete timer;
            return;
        }
        // 目标定时器处于链表的中间， 则把它们前后的定时器串联起来， 然后删除目标定时器
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    // SIGALRM信号每次被触发就在其信号处理函数（如果使用统一事件源，则是主函数）中执行一次tick函数
    // ，以处理链表上的到期任务
    void tick()
    {
        if(!head)
        {
            return;
        }
        printf("timer tick\n");
        time_t cur = time(NULL);
        util_timer* tmp = head;
        // 从头结点开始一次处理每个定时器，知道遇到一个尚未到期的定时器，这个就是定时器的核心逻辑
        while(tmp)
        {
            // 因为每个定时器都使用绝对时间作为超时值，所以我们可以把定时器的超时值直接和系统时间比较
            if(cur < tmp->expire)
            {
                break;
            }
            // 调用定时器的回调函数，执行定时任务
            tmp->cb_func(tmp->user_data);
            // 执行完定时器的任务后，就将它从链表中删除，并重置链表头节点
            head = tmp->next;
            if(head)
            {
                head->prev = nullptr;
            }
            delete tmp;
            tmp = head;
        }
    }
private:
    // 该函数表示它将目标定时器timer添加到lst_head之后的部分链表中
    void add_timer(util_timer* timer, util_timer* lst_head)
    {
        util_timer* prev = lst_head;
        util_timer* tmp = prev->next;
        // 遍历lst_head节点之后的部分链表，直到遇到超时时间大于目标定时器的超时时间
        while(tmp)
        {
            if(timer->expire < tmp->expire)
            {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        // 如果遍历完lst_head节点之后的部分链表，任未找到超时时间大于目标定时器的，则
        // 将目标定时器插入链表尾部，并把它设置为链表新的尾节点
        if(!tmp)
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = nullptr;
            tail = timer;
        }
    }
private:
    util_timer* head;   // 头节点
    util_timer* tail;   // 尾节点
};

#endif