#include <time.h>
#include "httpConn.h"
#include <iostream>
#ifndef HEAPTIMER_H
#define HEAPTIMER_H

class timer{
public:
    timer(){m_expire = 0;user_data=nullptr;}
    void init(int delay, http_conn* user);
    void init(int delay);
    ~timer(){}
    time_t get_expire(){
        return m_expire;};
    void (*cb_function)(timer*);
    static void cb_fun(timer*);
    void call_cb_fun(){
        if(!cb_function){
            printf("cb function is null\n");
            return;
        }
        else
            cb_function(this);
    }

private:
    time_t m_expire;
    http_conn* user_data;
};

class timerHeap{
public:
    timerHeap(int cap){
        heap = new timer*[cap];
        if(!heap){
            throw std::exception();
        }
        for(int i = 0; i < cap; i++){
            heap[i] = nullptr;
        }
        capcity = cap;
        cur_size = 0;
        
    };    
    timerHeap(timer** init_array, int size, int cap){
        heap = new timer*[cap];
        if(!heap){
            throw std::exception();
        }
        for(int i = 0; i < cap; i++){
            heap[i] = nullptr;
        }
        for(int i = 0; i < size; i++){
            heap[i] = init_array[i];
        }
        cur_size = size;
        for(int i = cur_size/2-1; i>=0; i--){
            percolate_down(i);
        }

    };
    ~timerHeap(){
        if(!heap){
            delete[] heap;
        }
    }
    void add_timer(timer*);
    void del_timer(timer*);
    void del_timer(int);
    bool empty() const;
    timer* top() const;
    void pop();
    void tick();

private:
    timer** heap;
    int capcity;
    int cur_size;
    void percolate_down(int i);
    void percolate_up(int i);
    // 扩容一倍
    void double_larger();
};

#endif