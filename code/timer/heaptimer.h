
#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {
    int id;     // �ļ�������
    TimeStamp expires;  // ��ʱʱ��
    TimeoutCallBack cb; // �ص���������CloseConn_�ر�����
    bool operator<(const TimerNode& t) {    // ���رȽ������
        return expires < t.expires;
    }
};
class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }

    ~HeapTimer() { clear(); }
    
    void adjust(int id, int newExpires);

    void add(int id, int timeOut, const TimeoutCallBack& cb);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int GetNextTick();

private:
    void del_(size_t i);    // ɾ��
    
    void siftup_(size_t i); // ����һ���ڵ㵽���֮��������������ϵ���λ�ã���֤С���ѵ�����

    bool siftdown_(size_t index, size_t n); // ���µ���λ��

    void SwapNode_(size_t i, size_t j); // ���������ڵ�

    std::vector<TimerNode> heap_;   // vectorʵ�ֵ�

    std::unordered_map<int, size_t> ref_;   // ����ֵ�������Ĺ�ϵ
};

#endif //HEAP_TIMER_H