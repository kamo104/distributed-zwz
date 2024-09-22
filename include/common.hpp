#pragma once

#include <mpi.h>
#include <queue>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>


class PacketChannel;
class State;
class Counter;
class LamportClock;

/* global variables */
extern int winAmount;
extern int cyclesNum;
extern int currentCycle;
extern int rank;
extern int size, guns;
extern int currPair;
extern int rollVal;

extern State currentState;
extern LamportClock clk;
extern PacketChannel roleQueue, gunQueue;
extern Counter roleCounter, gunCounter;
/* global variables */

/* channel stuff */
template<class T>
class Channel{
private:
  pthread_mutex_t mut;
  pthread_cond_t wc;
  pthread_mutexattr_t attr;
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
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    
    pthread_mutex_init(&mut, &attr);
    pthread_cond_init(&wc, NULL);
  }
  Channel(T init_val){
    Channel();
    data = init_val;
  }
  ~Channel(){
    pthread_mutex_destroy(&mut);
    pthread_cond_destroy(&wc);
    pthread_mutexattr_destroy(&attr);
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

std::string toString(PacketType pkt);

#pragma pack(push,1)
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
#pragma pack(pop)

std::string packetDump(const packet_t &pkt);

void sendPacket(packet_t *pkt, int destination, PacketType tag, bool increment=true);
/* packet stuff */

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
};
/* lamport here */

/* logging stuff */
#ifdef DEBUG
  #define debug(FORMAT,...) printf("%c[%d;%dm [%d,%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, clk.data, ##__VA_ARGS__, 27,0,37);
#else
  #define debug(...) ;
#endif
#define println(FORMAT,...) printf("%c[%d;%dm [%d,%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, clk.data, ##__VA_ARGS__, 27,0,37);
/* logging stuff */

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

std::string toString(StateType state);

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
    debug("przechodzę do stanu: %s", toString(newState).c_str());
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
};
/* state stuff */

class Counter: public Channel<int>{
public:
  std::vector<int> nack;

  int total;
  int allowedNack;

  Counter(){
    data =0;
  }
  Counter(int total, int allowedNack): total(total), allowedNack(allowedNack){
    data=0;
  }
  void reset(){
    lock();
    data = 0;
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
      data++;
    }
    signal();
    unlock();
  }
  void await(){
    lock();
    while(data + nack.size() < total) {
      wait();
    };
    unlock();
  }
  void awaitEntry(){
  	lock();
  	while(data + nack.size() < total && nack.size() > allowedNack) wait();
  	unlock();
  }
};

struct compare{
  bool operator()(packet_t a, packet_t b){
    return a.timestamp < b.timestamp;
  }
};

typedef std::priority_queue<packet_t, std::vector<packet_t>, compare> packet_pq;

class PacketChannel : public Channel<int>{
protected:
  std::vector<packet_t> pkts;
public:
  const std::vector<packet_t>& vec(){
    return pkts;
  }
  void push(packet_t pkt){
    lock();
    auto pos = std::lower_bound(pkts.begin(),pkts.end(),pkt,[](const packet_t& p1, const packet_t& p2){return p1.timestamp < p2.timestamp;});
    pkts.insert(pos,pkt);
    signal();
    unlock();
  }

  void pop(){
    lock();
    pkts.erase(pkts.begin());
    signal();
    unlock();
  }

  void clear(){
    lock();
    pkts.clear();
    signal();
    unlock();
  }

  const packet_t& findPkt(int src){
    auto it =  std::find_if(vec().begin(),vec().end(),[&](const packet_t& tmp){return tmp.src==src;});
    if(it != vec().end()){
      return it[0];
    }

    throw std::logic_error("packet with src: "+std::to_string(src)+" not found");
  }
};
