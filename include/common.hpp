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
extern int ackNum;
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
  ROLL, // imo rolling powinien być typem pakietu a nie stanem skoro wysyłamy go drugiemu procesowi
  END, // dodałem typ end który oznacza token w pakiecie
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

void sendPacket(packet_t *pkt, int destination, PacketType tag);
/* packet stuff */

/* state stuff */
enum StateType : int {
  INIT,
  WAIT_ROLE,
  ROLE_PICKED,
  WAIT_PAIR,
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

typedef Channel<std::vector<int>> vecInt;
typedef Channel<std::queue<packet_t>> packet_queue;

extern vecInt nackVec;
extern packet_queue waitQueue;

