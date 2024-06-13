#include <common.hpp>
#include <comms.hpp>
#include <pthread.h>
#include <util.hpp>
#include <chrono>

int winAmount = 0, currentCycle = 0;
int size, rank, guns, cyclesNum;
vecInt nackVec;
packet_queue waitQueue;

State currentState;
LamportClock clk;
Counter cnt;

void mainLoop(){
	//TODO: double check the loop end conditions, I don't think they makes sense
	while(currentState != FINISHED && currentCycle != cyclesNum-1){
		switch(currentState){
			case INIT : {
				break;
			}
			case WAIT_ROLE : {
				break;
			}
			case ROLE_PICKED : {
				break;
			}
			case WAIT_PAIR : {
				break;
			}
			case ROLLING : {
				break;
			}
			case WAIT_END : {
				break;
			}
			case FINISHED : {
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
  check_thread_support(provided);

  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  auto now = std::chrono::system_clock::now().time_since_epoch();
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

  srand(rank*now_ms);

  // init licznika
  cnt = Counter(size-1, size/2 - 1);

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
