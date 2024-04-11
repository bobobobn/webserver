#include "heapTimer.h"
#include <algorithm>

static void swap(timer* A, timer* B){
    timer* temp;
    temp = A;
    A = B;
    B = temp;
}

void timer::cb_fun(timer * timer_this){
    timer_this->user_data->close_conn(true);
}

void timer::init(int delay, http_conn* user){    
        if(!user)
            throw std::exception();    
        m_expire = time(NULL) + delay;
        user_data = user;
        cb_function = cb_fun;
}

void timer::init(int delay){     
    if(!user_data)
        throw std::exception();
    m_expire = time(NULL) + delay;
    cb_function = cb_fun;
}

void timerHeap::percolate_down(int i){
    while( i < cur_size - 1 ){
        int next_index = (heap[2*i+1]->get_expire() < heap[2*i+2]->get_expire()) ? 2*i+1 : 2*i+2;
        if(heap[next_index]->get_expire() < heap[i]->get_expire()){
            swap(heap[next_index], heap[i]);
            i = next_index;
        }
        else{
            break;
        }
    }
}

void timerHeap::percolate_up(int i){
    while( (i > 0) && (heap[(i-1)/2]->get_expire() > heap[i]->get_expire()) ){
        swap(heap[(i-1)/2], heap[i]);
        i = (i-1)/2;
    }
}

void timerHeap::add_timer(timer* timer_to_add){
    if(!timer_to_add)
        return;
    if(cur_size >= capcity){
        double_larger();
    }
    heap[cur_size] = timer_to_add;
    percolate_up(cur_size);
    ++cur_size;
    timer_to_add->cb_function = timer::cb_fun;
}

void timerHeap::double_larger(){
    timer** temp = new timer*[capcity*2];
    if(!temp)
        throw std::exception();
    capcity *= 2;
    for(int i = 0; i < capcity; i++){
        temp[i] = nullptr;
    }
    for(int i = 0; i < cur_size; i++){
        temp[i] = heap[i];
    }
    delete[] heap;
    heap = temp;
}

void timerHeap::del_timer(timer* timer_to_del){
    for(int i = 0; i < cur_size; i++){
        if( heap[i] == timer_to_del ){
            del_timer(i);
            break;
        }
    }
}

void timerHeap::del_timer(int timer_idx){
    if(timer_idx >= cur_size){
        throw std::exception();
    }
    // delete heap[timer_idx];
    heap[timer_idx]->cb_function = nullptr;
    heap[timer_idx] = heap[cur_size-1];
    heap[cur_size-1] = nullptr;
    cur_size--;
    percolate_down(timer_idx);
}

bool timerHeap::empty() const{
    return cur_size == 0;
}

timer* timerHeap::top() const{
    if(!empty())
        return heap[0];
    return nullptr;
}

void timerHeap::pop(){
    del_timer(0);
}

void timerHeap::tick(){
    timer* root = top();
    if(!root)
        return;
    if(time(NULL) >= root->get_expire()){
        root->call_cb_fun();
        pop();
        if(!empty())
            alarm(top()->get_expire() - time(NULL));
    }

}