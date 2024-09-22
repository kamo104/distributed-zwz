#include <common.hpp>
#include <comms.hpp>
#include <cstdlib>
#include <mpi.h>
#include <util.hpp>
#include <chrono>
#include <ranges>

/* global variables */
int winAmount = 0, currentCycle = 0;
int size, rank, guns, cyclesNum;
int currPair, highestPriorityID;
int rollVal = -1;
PacketChannel waitQueue;

State currentState;
LamportClock clk;
Counter cnt;
/* global variables */

void mainLoop(){
	packet_t tmp;
	int my_id;
	int pair_id;
	while(currentState != FINISHED && currentCycle != cyclesNum-1){
		switch(currentState){
			case INIT : {
			  cnt = Counter(size-1, size/2 - 1);
			  cnt.lock();
				// debug("przechodzę do stanu WAIT_ROLE");
			  currentState.changeState(WAIT_ROLE);
			  debug("ubiegam się o dostęp do sekcji krytycznej zabójców");
			  for(int dst : std::ranges::iota_view(0,size)){
			  	if(dst==rank) continue;
			  	sendPacket(&tmp, dst, REQ, false);
			  }
			  waitQueue.push(tmp);
			  clk++;
			  cnt.unlock();
				break;
			}
			case WAIT_ROLE : {
				cnt.await();
				highestPriorityID = waitQueue.vec()[0].src;

				// TODO: check/remove possibly erroneus mutex lock
				currentState.lock();

				for(int i=0;i<waitQueue.vec().size();i++){
					if(waitQueue.vec()[i].src!=rank)continue;
					my_id = i;
				}
				int size2 =waitQueue.vec().size()/2;
				pair_id = size2 > my_id ? size2+my_id : my_id-size2;

				currPair = waitQueue.vec()[pair_id].src;

				// if I'm the killer send a pair req and set cnt to killer mode
				// TODO: double check if this has a possibility of firing off before other processes get their counter swapped
				if(my_id<pair_id){
					cnt = Counter(size/2-1,guns);
					debug("wysyłam PAIR REQ");
					sendPacket(NULL, currPair, PAIR);
				}
				currentState.changeState(WAIT_PAIR);
				currentState.unlock();
				break;
			}
			// case ROLE_PICKED : {
			// 	break;
			// }
			case WAIT_PAIR : {
				currentState.await();
				// send gun requests if I'm a killer
				// for(const packet_t& pkt:waitQueue.vec().){
				// }
				break;
			}
			case ROLLING : {
				debug("rzut kością")
				rollVal = random()%INT32_MAX;
				tmp.value = rollVal;
				sendPacket(&tmp, currPair, ROLL);
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
			case WAIT_GUN: {
				cnt.await_entry(); // wait until critical section entry
				currentState.changeState(ROLLING);
				// TODO: give back the gun right away to the first nack we sent/noone
				// sendPacket(tmp, , )
				break;
			}
    }
  }
}

int main(int argc, char** argv) {
  for(int i=0;i<argc;i++){
    char* tmp = argv[i];
    if(!strcmp(tmp,"-g") || !strcmp(tmp,"--guns")){ // guns num
      guns = atoi(argv[++i]);
    }
    else if(!strcmp(tmp,"-c") || !strcmp(tmp,"--cycles")){ // cycles num
      cyclesNum = atoi(argv[++i]);
    }
  }
  
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
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
