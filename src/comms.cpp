#include <common.hpp>
#include <comms.hpp>

packet_t tmp;

/* communication thread */
void* CommThread::start(void* ptr){
  MPI_Status status;
  while(currentState != FINISHED && 
    currentCycle != cyclesNum-1)
  {
    MPI_Recv(&tmp, 1, MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    switch(tmp.type){
      case ACK : {
        switch(currentState){
          case(WAIT_ROLE):{
            break;
          }
        }
        break;
      }
      case REQ : {
        break;
      }
      case NACK : {
        break;
      }
      case RELEASE : {
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

