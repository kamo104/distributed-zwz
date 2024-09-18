#pragma once

#include <mpi.h>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <vector>
#include <algorithm>


/* global variables */
extern int winAmount;
extern int cyclesNum;
extern int currentCycle;
extern int rank;
extern int size, guns;
extern int currPair;
/* global variables */

/* logging stuff */
#ifdef DEBUG
  #define debug(FORMAT,...) printf("%c[%d;%dm [%d,%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, clk.data, ##__VA_ARGS__, 27,0,37);
#else
  #define debug(...) ;
#endif
#define println(FORMAT,...) printf("%c[%d;%dm [%d,%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, clk.data, ##__VA_ARGS__, 27,0,37);
/* logging stuff */


/* channel stuff */
template<class T>
class Channel{
private:
  pthread_mutex_t mut;
  pthread_cond_t wc;
public:
  T data;

  void lock(){
    pthread_mutex_lock(&mut);
  }
  void unlock(){
    pthread_mutex_unlock(&mut);
  }

  // lock - zmiana/odebranie danych - signal - unlock
  void signal(){
    pthread_cond_signal(&wc);
  }
  // lock - wait - odczyt danych - unlock
  void wait(){
    pthread_cond_wait(&wc, &mut);
  }
  
  Channel(){
    pthread_mutex_init(&mut, NULL);
    pthread_cond_init(&wc, NULL);
  }
  Channel(T init_val){
    Channel();
    data = init_val;
  }
  ~Channel(){
    pthread_mutex_destroy(&mut);
    pthread_cond_destroy(&wc);
  }
};
/* channel stuff */

/* packet stuff */
enum PacketType : int{
  REQ,
  ACK,
  NACK,
  RELEASE,
  PAIR,
  GUN,
  ROLL,
  END,
};

struct packet_t {
  // type jest nadmiarowe jeśli nie będziemy przesyłać pakietów między wątkami
  PacketType type;

  int timestamp;

  // src i dst to samo co type
  int src; 
  int dst;

  // używane w tokenie i rolling
  int value;
};

void sendPacket(packet_t *pkt, int destination, PacketType tag, bool increment=true);
/* packet stuff */

/* state stuff */
enum StateType : int {
  INIT,
  WAIT_ROLE,
  ROLE_PICKED,
  WAIT_PAIR,
  WAIT_GUN,
  ROLLING,
  WAIT_END,
  FINISHED,
};

class State : public Channel<StateType>{
public:
  bool operator==(State second){
    return data == second.data;
  }
  bool operator==(StateType second){
    return data == second;
  }
  bool operator!=(State second){
    return data != second.data;
  }
  bool operator!=(StateType second){
    return data != second;
  }
  operator StateType() const {
    return data;
  }

  void changeState(StateType newState){
    lock();
    if(data == FINISHED){
      unlock();
      return;
    }
    data = newState;
    signal();
    unlock();
    return;
  }

  void await(){
    lock();
    wait();
    unlock();
  }

  State(){
    data = INIT;
  }
} extern currentState;
/* state stuff */

/* lamport here */
class LamportClock : public Channel<int>{
public:
  int update(int recv_timestamp){
    lock();
    data = std::max(recv_timestamp, data)+1;
    unlock();
    return data;
  }
  LamportClock operator++(int){
    lock();
    LamportClock tmp = *this;
    data++;
    unlock();
    return tmp;
  }
} extern clk;
/* lamport here */

class Counter: public Channel<int>{
public:
  int ack = 0;
  std::vector<int> nack;

  int total;
  int allowed_nack;

  Counter(){}
  Counter(int n_responses, int nack_threshold){
    total = n_responses;
    allowed_nack = nack_threshold;
  }
  void reset(){
    lock();
    ack = 0;
    nack.clear();
    unlock();
  }
  void incrACK(){
    lock();
    data++;
    signal();
    unlock();
  }
  void incrNACK(int src){
    lock();
    nack.push_back(src);
    signal();
    unlock();
  }
  void convert(int src){
    lock();
    auto it = std::find(nack.begin(),nack.end(),src);
    if(it != nack.end()){
      nack.erase(it);
      ack++;
    }
    signal();
    unlock();
  }
  void await(){
    lock();
    while(ack + nack.size() < total) wait();
    unlock();
  }
  void await_entry(){
	lock();
	while(ack + nack.size() < total || nack.size() > nack_threshold) wait();
	unlock();
  }
};

struct compare{
  bool operator()(packet_t a, packet_t b){
    return a.timestamp < b.timestamp;
  }
};

typedef std::priority_queue<packet_t, std::vector<packet_t>, compare> packet_pq;

class PacketChannel : packet_pq, public Channel<int>{
public:
  std::vector<packet_t>& vec(){
    return packet_pq::c;
  }
  void push(packet_t pkt){
    lock();
    packet_pq::push(pkt);
    signal();
    unlock();
  }
  void pop(){
    lock();
    packet_pq::pop();
    signal();
    unlock();
  }
  void clear(){
    lock();
    while(!packet_pq::empty()){
      packet_pq::pop();
    }
    signal();
    unlock();
  }
};

extern PacketChannel waitQueue;

extern Counter cnt;
