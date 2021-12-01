/*
    时间轮以恒定的速度顺时间转动，每转动一步就指向下一个槽，每次转动称为一个滴答（tick）。
    一个滴答的时间称为时间轮的槽间隔SI（slot interval），它实际上就是心搏时间。
    时间轮共有N个槽，因此它转动一周的时间为N*SI，每个槽指向一条定时器链表，每条链表上的定时器具有
    相同的特征：它们的定时时间相差N*SI的整数倍。时间轮正是利用这个关系将定时器散列到不同链表中。
    假如现在指针指向槽cs，我们要添加一个定时时间为ti的定时器，则该定时器将被插入槽ts（timer slot）对应
    的链表中：ts=(cs+(ti/si))%N

    时间轮利用哈希表的思想，将定时器散列到不同的链表上，这样每条链表上的定时器数目都将明显少于原来排序链表
    上的定时器数目，插入操作的效率基本不受定时器数目的影响。对于时间轮而言，要提高定时精度，就要使SI值足够小；
    要提高执行效率，则要求N值足够大。

    从执行效率来看：
        添加一个定时器的时间复杂度O(1)
        删除一个定时器的时间复杂度O(1)
        执行一个定时器的时间复杂度O(n)，但实际上执行要比O(n)好得多，因为时间轮的槽越多，等价于散列表的入口越多，
        从而每条链表上的定时器越少，此外代码使用的是一个时间轮，如果多个轮子来实现时间轮，执行定时器的时间复杂度
        接近O(1)。
*/

#ifndef TIME_WHEEL_TIMER_H
#define TIME_WHEEL_TIMER_H

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>

#define BUFFER_SIZE 64
class tw_timer;

// 用户数据结构
struct client_data
{
    sockaddr_in address;    // 客户端socket地址
    int sockfd;             // socket文件描述符
    char buf[BUFFER_SIZE];  // 读缓存
    tw_timer* timer;      // 定时器
};

// 定时器类
class tw_timer
{
public:
    tw_timer(int rot, int ts) : next(nullptr), prev(nullptr), rotation(rot), time_slot(ts) {}
public:
    int rotation;                   // 记录定时器在时间轮转多少圈后生效
    int time_slot;                  // 记录定时器属于时间轮上哪个槽（对应的链表）
    void (*cb_func)(client_data*);  // 定时器回调函数
    client_data* user_data;         // 客户数据
    tw_timer* next;                 // 指向下一个定时器
    tw_timer* prev;                 // 指向前一个定时器
};

// 时间轮类
class time_wheel
{
public:
    time_wheel() : cur_slot(0)
    {
        for(int i = 0; i < N; ++i)
        {
            // 初始化每一个槽的头节点
            slots[i] = nullptr;
        }
    }

    ~time_wheel()
    {
        // 遍历每一个槽，并销毁其中的定时器
        for(int i = 0; i < N; ++i)
        {
            tw_timer* tmp = slots[i];
            while(tmp)
            {
                slots[i] = tmp->next;
                delete tmp;
                tmp = slots[i];
            }
        }
    }

    // 根据定时值timeout创建一个定时器，并把它插入合适的槽中
    tw_timer* add_timer(int timeout)
    {
        if(timeout < 0)
        {
            return nullptr;
        }
        int ticks = 0;
        /*
        下面根据待插入定时器的超时值计算它将在时间轮转动多少个滴答后被触发，并将该滴答数存储在ticks中。
        如果待插入定时器的超时值小于时间轮的槽间隔SI，则将ticks向上折合为1，否则就将ticks向下折合为timeout/SI
        */
        if(timeout < SI)
        {
            ticks = 1;
        }
        else
        {
            ticks = timeout / SI;
        }
        // 计算待插入的定时器在时间轮转动多少圈后被触发
        int rotation = ticks / N;
        // 计算待插入的定时器应该被插入哪个槽中
        int ts = (cur_slot + (ticks % N));
        // 创建新的定时器，它在时间轮转动rotation圈之后被触发，且处于第ts槽中
        tw_timer* timer = new tw_timer(rotation, ts);
        // 如果第ts个槽中尚无任何定时器，则把新建的定时器插入其中，并将该定时器设置为该槽头结点
        if(!slots[ts])
        {
            printf("add timer, rotation is %d, ts is %d, cur_slot is %d\n", rotation, ts, cur_slot);
            slots[ts] = timer;
        }
        // 否则，将定时器插入第ts个槽中
        else
        {
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }
        return timer;
    }

    // 删除目标定时器timer
    void del_timer(tw_timer* timer)
    {
        if(!timer)
        {
            return;
        }
        int ts = timer->time_slot;
        // slots[ts]是目标定时器所在槽的头结点。如果目标定时器就是该头结点，则需要重置第ts个槽的头结点
        if(timer == slots[ts])
        {
            slots[ts] = slots[ts]->next;
            if(slots[ts])
            {
                slots[ts]->prev = nullptr;
            }
            delete timer;
        }
        else
        {
            timer->prev->next = timer->next;
            if(timer->next)
            {
                timer->next->prev = timer->prev;
            }
            delete timer;
        }
    }

    // SI时间到后，调用该函数，时间轮向前滚动一个槽的间隔
    void tick()
    {
        // 取得时间轮上当前槽的头结点
        tw_timer* tmp = slots[cur_slot];
        printf("current slot is %d\n", cur_slot);
        while(tmp)
        {
            printf("tick the timer once\n");
            // 如果定时器的rotation值大于0，则它在这一轮不起作用
            if(tmp->rotation > 0)
            {
                --tmp->rotation;
                tmp = tmp->next;
            }
            // 否则，说明定时器已经到期，可以执行定时任务，然后删除该定时器
            else
            {
                tmp->cb_func(tmp->user_data);
                if(tmp == slots[cur_slot])
                {
                    printf("delete header in cur_slot\n");
                    slots[cur_slot] = tmp->next;
                    delete tmp;
                    if(slots[cur_slot])
                    {
                        slots[cur_slot]->prev = nullptr;
                    }
                    tmp = slots[cur_slot];
                }
                else
                {
                    tmp->prev->next = tmp->next;
                    if(tmp->next)
                    {
                        tmp->next->prev = tmp->prev;
                    }
                    tw_timer* tmp2 = tmp->next;
                    delete tmp;
                    tmp = tmp2;
                }
            }
        }
        // 更新时间轮的当前槽，以反映时间轮的转动
        cur_slot = ++cur_slot % N;
    }

private:
    static const int N = 60;    // 时间轮上槽的数目
    static const int SI = 1;    // 每SI秒时间轮转动一次，即槽间隔为SI
    tw_timer* slots[N];         // 时间轮的槽，其中每个元素指向一个定时器链表，链表无序
    int cur_slot;               // 时间轮的当前槽
};

#endif