#pragma once

#include <cstdint>
#include <mpi.h>
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
// class Counter;
class LamportClock;

/* global variables */
extern int winAmount;
extern int cyclesNum;
extern int currentCycle;
extern int rank;
extern int size, guns;
extern int currPair;
extern int rollVal;
extern bool killer;
extern int highestPriorityID;

extern State currentState;
extern LamportClock clk;
extern PacketChannel roleChannel, gunChannel;
// extern Counter roleCounter, gunCounter;

void init();
bool endCondition();
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
enum PacketType : uint8_t{
  // role selection
  ROLE,
  ROLE_ACK,
  ROLE_NACK,

  // pairing
  PAIR,
  PAIR_ACK,
  PAIR_NACK,

  // gun aquisition
  GUN,
  GUN_ACK,
  GUN_NACK,
  RELEASE,

  // rolling
  ROLL,

  // ending
  END,
  SCORE,
};
bool isACK(PacketType pkt);
bool isNACK(PacketType pkt);
PacketType toACK(PacketType pkt);
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

  // token ustalania wyniku
  int topScore;
  int topId;
};
#pragma pack(pop)

bool compare(const packet_t& p1, const packet_t& p2);

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
enum StateType : uint8_t {
  INIT,
  WAIT_ROLE,
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
    debug("przechodzę do stanu %s", toString(newState).c_str());
    lock();
    data = newState;
    signal();
    unlock();
    return;
  }

  void await(int n_unlock=0){
    lock();
    StateType oldState = data;
  	for(int i=0;i<n_unlock;i++) unlock();
    while(oldState==data) wait();
    unlock();
  }

  State(){
    data = INIT;
  }
};
/* state stuff */

// class Counter: public Channel<int>{
// public:
//   std::vector<int> nack;

//   int total;
//   int allowedNack;

//   Counter(){
//     data =0;
//   }
//   Counter(int total, int allowedNack): total(total), allowedNack(allowedNack){
//     data=0;
//   }
//   void incrACK(){
//     lock();
//     data++;
//     signal();
//     unlock();
//   }
//   void incrNACK(int src){
//     lock();
//     nack.push_back(src);
//     signal();
//     unlock();
//   }
//   // void convert(int src){
//   //   lock();
//   //   auto it = std::find(nack.begin(),nack.end(),src);
//   //   if(it != nack.end()){
//   //     nack.erase(it);
//   //     data++;
//   //   }
//   //   signal();
//   //   unlock();
//   // }
// };

class PacketChannel : public Channel<std::vector<packet_t>> {
protected:
  std::vector<packet_t> resp;
  int total;
  int allowedNack;
public:
  PacketChannel(){}
  PacketChannel(int total, int allowedNack): total(total), allowedNack(allowedNack){}
  std::vector<packet_t>& queue(){
    return data;
  }
  std::vector<packet_t>& responses(){
    return resp;
  }
  void qpush(packet_t pkt){
    lock();
    auto pos = std::lower_bound(
      data.begin(),
      data.end(),
      pkt,
      compare
    );
    data.insert(pos,pkt);
    signal();
    unlock();
  }
  void rpush(packet_t pkt){
    lock();
    resp.push_back(pkt);
    signal();
    unlock();
  }

  void qremove(int src){
    lock();
    data.erase(
      std::remove_if(
        data.begin(),
        data.end(),
        [src](const packet_t& pkt){
          return pkt.src==src;
        }
      ),
      data.end()
    );
    signal();
    unlock();
  }

  void rconvert(int src){
    lock();
    auto it = std::find_if(resp.begin(),resp.end(), [src](const packet_t& pkt){
      return pkt.src ==src;
    });
    if(it != resp.end()){
      it->type = toACK(it->type);
    }
    signal();
    unlock();
  }

  void dump(){
		std::ostringstream oss;
		oss << "queue: ";
		for(int i=0;i<data.size();i++){
			oss << "from: " << data[i].src << " type: " << toString(data[i].type) << " ; ";
		}
		oss << "\n responses: ";
		for(int i=0;i<resp.size();i++){
			oss << "from: " << resp[i].src << " type: " << toString(resp[i].type) << " ; ";
		}
		debug("%s",oss.str().c_str())
  }

  void clear(){
    lock();
    data.clear();
    resp.clear();
    signal();
    unlock();
  }
  int countNack(){
    return std::count_if(resp.begin(),resp.end(),[](const packet_t& pkt){
      return isNACK(pkt.type);
    });
  }
  int countAck(){
    return std::count_if(resp.begin(),resp.end(),[](const packet_t& pkt){
      return isACK(pkt.type);
    });
  }
  int rcountFrom(int src){
    return std::count_if(resp.begin(),resp.end(),[src](const packet_t& pkt){
      return pkt.src == src;
    });
  }
  int qcountFrom(int src){
    return std::count_if(data.begin(),data.end(),[src](const packet_t& pkt){
      return pkt.src == src;
    });
  }
  packet_t* qgetFrom(int src){
    auto it = std::find_if(data.begin(),data.end(),[src](const packet_t& pkt){
      return pkt.src == src;
    });
    if (it != data.end()) return it.base();
    return NULL;
  }
  packet_t* rgetFrom(int src){
    auto it = std::find_if(resp.begin(),resp.end(),[src](const packet_t& pkt){
      return pkt.src == src;
    });
    if (it != data.end()) return it.base();
    return NULL;
  }
  int qgetIndex(int src){
    auto it = std::find_if(data.begin(),data.end(),[src](const packet_t& pkt){
      return pkt.src == src;
    });
    return std::distance(data.begin(),it);
  }
  int rgetIndex(int src){
    auto it = std::find_if(resp.begin(),resp.end(),[src](const packet_t& pkt){
      return pkt.src == src;
    });
    return std::distance(resp.begin(),it);
  }

  void awaitRole(int n_unlock=0){
    lock();
  	for(int i=0;i<n_unlock;i++) unlock();
  	while(
  	  countAck() + countNack() < size-1 || 
  	  data.size() < size) wait();
  	unlock();
  }

  void awaitGun(int n_unlock=0){
  	lock();
  	for(int i=0;i<n_unlock;i++) unlock();
  	while(
  	  countAck() + countNack() < size/2-1 || 
  	  countNack() > allowedNack) wait();
  	unlock();
  }

  // void await(){
  // 	lock();
  // 	while(
  // 	  countAck() + countNack() < total-1 || 
  // 	  countNack() > allowedNack || 
  // 	  data.size() < total) wait();
  // 	unlock();
  // }
};
