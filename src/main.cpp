#include <common.hpp>
#include <comms.hpp>
#include <cstdlib>
#include <mpi.h>
#include <string>
#include <util.hpp>
#include <chrono>
#include <ranges>

/* global variables */
// out stats
int winAmount = 0, currentCycle = 0;
// input vars
int size, rank, guns, cyclesNum;
// vars
int currPair;
bool killer;
int highestPriorityID;
int rollVal;
// classes
PacketChannel roleChannel, gunChannel;
// Counter roleCounter, gunCounter;

State currentState;
LamportClock clk;
/* global variables */

void mainLoop(){
	packet_t tmp;
	int myId;
	int pairId;
	while(!endCondition()){
		switch(currentState){
			case INIT : {
			  roleChannel.lock();
			  currentState.changeState(WAIT_ROLE);
			  // debug("ubiegam się o dostęp do sekcji krytycznej zabójców");
			  clk.lock();
			  for(int dst : std::ranges::iota_view(0,size)){
			  	if(dst==rank) continue;
			  	sendPacket(&tmp, dst, ROLE, false);
			  }
			  roleChannel.qpush(tmp);
			  clk++;
			  clk.unlock();
			  roleChannel.unlock();
				break;
			}
			case WAIT_ROLE : {
				// BUG: we can get an ACK before someone sends 
				//      us a ROLE_REQ. Make sure roleQueue is full.
				// FIX: merge roleCounter and roleQueue
				// FIXED: ??? error doesn't appear anymore
				roleChannel.awaitRole();
				roleChannel.lock();
				highestPriorityID = roleChannel.queue()[0].src;

				// determine index of my role_REQ
				myId = roleChannel.qgetIndex(rank);

				int size2 = roleChannel.queue().size()/2;

				if(size2>myId) killer = true;
				else killer = false;

				// determine pairRank
				pairId = killer ? size2+myId : myId-size2;

				currPair = roleChannel.queue()[pairId].src;
				roleChannel.unlock();
				// roleChannel.dump();
				// debug("pairRank: %d",currPair);
				std::ostringstream oss;
				oss << "size2: " << size2 << " myId: " << myId <<
				" pairId: " << pairId;
				debug("pairing: %s",oss.str().c_str())

				// if I'm the killer send a pair req
				currentState.changeState(WAIT_PAIR);
				break;
			}
			case WAIT_PAIR : {
				// wait untill we get a PAIR_ACK
				currentState.lock();
				if(myId<pairId){
					sendPacket(NULL, currPair, PAIR);
				}
				currentState.await(1);
				break;
			}
			case WAIT_GUN: {
				// send gun requests
				roleChannel.lock();
				gunChannel.lock();
				clk.lock();
				for(int i=0;i<roleChannel.queue().size()/2;i++){
					if(roleChannel.queue()[i].src==rank) continue;
					sendPacket(&tmp, roleChannel.queue()[i].src, GUN, false);
				}
				clk++;
				clk.unlock();
				gunChannel.qpush(tmp);
				roleChannel.unlock();

				// wait untill we get a gun
				gunChannel.awaitGun(1);
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
				++currentCycle;
				// if finished last cycle and had highest priority last cycle
				if (endCondition() && highestPriorityID == rank){
					tmp.topScore = winAmount;
					tmp.topId = rank;
					tmp.value = 0;
					// debug("rozpoczynam zliczanie punktów i koniec rundy");
					sendPacket(&tmp, (rank+1)%size, SCORE);
					break;
				}
				// program end detection
				if(endCondition()){
					break;
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

  clk.data = random()%(size*2)+rank;
  // clk.data = rank;
  init();
  
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
