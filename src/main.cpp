#include <common.hpp>
#include <comms.hpp>
#include <cstdlib>
#include <mpi.h>
#include <string>
#include <util.hpp>
#include <chrono>
#include <ranges>

/* global variables */
int winAmount = 0, currentCycle = 0;
int size, rank, guns, cyclesNum;
int currPair = -1;
bool killer = false;
int highestPriorityID;
int rollVal = -1;
PacketChannel roleQueue, gunQueue;
Counter roleCounter, gunCounter;

State currentState;
LamportClock clk;
/* global variables */

void mainLoop(){
	packet_t tmp;
	int myId;
	int pairId;
	while(currentState != FINISHED && currentCycle != cyclesNum-1){
		switch(currentState){
			case INIT : {
			  roleCounter.lock();
			  roleQueue.lock();
			  currentState.changeState(WAIT_ROLE);
			  // debug("ubiegam się o dostęp do sekcji krytycznej zabójców");
			  clk.lock();
			  for(int dst : std::ranges::iota_view(0,size)){
			  	if(dst==rank) continue;
			  	sendPacket(&tmp, dst, ROLE, false);
			  }
			  roleQueue.push(tmp);
			  clk++;
			  clk.unlock();
			  roleQueue.unlock();
			  roleCounter.unlock();
				break;
			}
			case WAIT_ROLE : {
				roleCounter.await();
				highestPriorityID = roleQueue.vec()[0].src;

				roleQueue.lock();
				// determine index of my role_REQ
				auto it = std::find_if(roleQueue.vec().begin(),roleQueue.vec().end(),[](const packet_t& pkt){return pkt.src==rank;});
				myId = std::distance(roleQueue.vec().begin(),it);

				int size2 = roleQueue.vec().size()/2;

				if(size2>myId) killer = true;
				else killer = false;

				// determine pairRank
				pairId = killer ? size2+myId : myId-size2;

				currPair = roleQueue.vec()[pairId].src;
				roleQueue.unlock();
				debug("pairRank: %d",currPair);

				// if I'm the killer send a pair req
				if(myId<pairId){
					sendPacket(NULL, currPair, PAIR);
				}
				currentState.changeState(WAIT_PAIR);
				break;
			}
			case WAIT_PAIR : {
				// wait untill we get a PAIR_ACK
				currentState.await();
				break;
			}
			case WAIT_GUN: {
				// send gun requests
				clk.lock();
				for(int i=0;i<roleQueue.vec().size()/2;i++){
					if(roleQueue.vec()[i].src==rank) continue;
					sendPacket(&tmp, roleQueue.vec()[i].src, GUN, false);
				}
				gunQueue.push(tmp);
				clk++;
				clk.unlock();

				// wait untill we get a gun
				gunCounter.awaitEntry();
				currentState.changeState(ROLLING);
				break;
			}
			case ROLLING : {
				if(!killer) {
					currentState.await();
					break;
				}
				// send ROLL
				rollVal = random()%INT32_MAX;
				tmp.value = rollVal;
				sendPacket(&tmp, currPair, ROLL);
				clk.lock();

				// give back the gun
				for(int i=0;i<roleQueue.vec().size()/2;i++){
					if(roleQueue.vec()[i].src==rank) continue;
					sendPacket(&tmp, roleQueue.vec()[i].src, RELEASE, false);
				}
				gunQueue.lock();
				gunQueue.remove(rank);
				gunQueue.unlock();
				clk++;
				clk.unlock();
				currentState.await();

				break;
			}
			case WAIT_END : {
				// if highest priority begin end cycle barrier
				if(highestPriorityID == rank){
					tmp.value = 0;
					sendPacket(&tmp, (rank+1)%size, END);
				}
				currentState.await();
				break;
			}
			case FINISHED : {
				// if finished last cycle and had highest priority last cycle
				if (++currentCycle == cyclesNum && highestPriorityID == rank){
					tmp.topScore = winAmount;
					tmp.topId = rank;
					tmp.value = 0;
					debug("rozpoczynam zliczanie punktów i koniec rundy");
					sendPacket(&tmp, (rank+1)%size, SCORE);
					return;
				}
				currentState.changeState(INIT);
				break;
			}
    }
  }
}

int main(int argc, char** argv) {
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  for(int i=0;i<argc;i++){
    char* tmp = argv[i];
    if(!strcmp(tmp,"-g") || !strcmp(tmp,"--guns")){ // guns num
      guns = atoi(argv[++i]);
    }
    else if(!strcmp(tmp,"-c") || !strcmp(tmp,"--cycles")){ // cycles num
      cyclesNum = atoi(argv[++i]);
    }
  }
  
  // check_thread_support(provided);

  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  auto now = std::chrono::system_clock::now().time_since_epoch();
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

  srand(rank*now_ms);

  // init licznika
  // cnt = Counter(size-1, size/2 - 1);

  // clk.data = random()%size+rank;
  clk.data = rank;
  roleCounter = Counter(size-1, size-1);
	gunCounter = Counter(size/2-1,guns-1);
  
  // dodanie kolejnego bloku bo CommThread w destruktorze czeka na zakończenie pracy wątku
  {
    CommThread commThread;
    commThread.begin();

    // TODO: main stuff
    mainLoop();
  }

  MPI_Finalize();
  return 0;
}
