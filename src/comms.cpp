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
  	// update Lamport Clock
  	clk.update(tmp.timestamp);
    switch(tmp.type){
      case ACK : {
        switch(currentState){
          case WAIT_ROLE : case WAIT_GUN :{
            cnt.incrACK();
            break;
          }
          case WAIT_PAIR :{
            currentState.changeState(WAIT_GUN);
            break;
          }
        }
        break;
      }
      case REQ : {
        // TODO: add to waitQueue?
        if(tmp.timestamp<clk.data 
            && currentState <= ROLLING){
          // send nack
          sendPacket(&tmp, tmp.src, NACK);
          break;
        }
        // send ack
        sendPacket(&tmp,tmp.src,ACK);
        break;
      }
      case NACK : {
        cnt.incrNACK(tmp.src);
		    break;
      }
      case RELEASE : {
        cnt.convert(tmp.src);
		    break;
      }
      case ROLL : {
        break;
      }
      case END : {
        currentState.lock();
        if(currentState >= WAIT_END){
          // send END to next in the ring
          sendPacket(&tmp,(rank+1)%size, END);
          currentState.unlock();
          break;
        }
        sendPacket(&tmp, tmp.value, NACK);
        currentState.unlock();
        break;
      }
      case GUN:
        

        break;
      case PAIR:
        break;
      }
  }
  pthread_exit(NULL);
}
/* communication thread */

