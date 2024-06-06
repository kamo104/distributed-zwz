#include <common.hpp>
#include <comms.hpp>

packet_t tmp;

int findInVec(int id){
	for(int i=0;i<nackVec.size();i++){
		if(nackVec[i] == id) return i;
	}
	return -1;
}

/* communication thread */
void* CommThread::start(void* ptr){
  MPI_Status status;
  while(currentState != FINISHED && 
    currentCycle != cyclesNum-1)
  {
    MPI_Recv(&tmp, 1, MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	// update Lamport Clock
	clk.update(tmp.timestamp);
    switch(tmp.type){
      case ACK : {
        cnt.incrACK();
		break;
      }
      case REQ : {
        break;
      }
      case NACK : {
        cnt.incrNACK();
		break;
      }
      case RELEASE : {
		if(int i = findInVec(tmp.src)){
			nackVec.erase(nackVec.begin() + i);
			cnt.convert();
		}
		break;
      }
      case ROLLING : {
        break;
      }
      case END : {
        break;
      }
    }
  }
  pthread_exit(NULL);
}
/* communication thread */

