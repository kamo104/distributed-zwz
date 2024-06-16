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
int currPair;
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

				currentState.lock();

				for(int i=0;i<waitQueue.vec().size();i++){
					if(waitQueue.vec()[i].src!=rank)continue;
					my_id = i;
				}
				int size2 =waitQueue.vec().size()/2;
				pair_id = size2 > my_id ? size2+my_id : my_id-size2;

				currPair = waitQueue.vec()[pair_id].src;

				
				// if I'm the killer send a pair req and set cnt to killer mode
				if(my_id<pair_id){
					debug("wysyłam PAIR REQ");
					sendPacket(NULL, currPair, PAIR);
					cnt = Counter(size/2-1,guns);
				}
				currentState.changeState(WAIT_PAIR);
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
				tmp.value = random()%INT32_MAX;
				sendPacket(&tmp, currPair, ROLL);
				break;
			}
			case WAIT_END : {
				break;
			}
			case FINISHED : {
				break;
			}
      case WAIT_GUN: {
      	cnt.await();
      	currentState.changeState(ROLLING);
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
