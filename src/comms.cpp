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
            ackNum++;
            nackVec.lock();
            nackVec.signal();
            nackVec.unlock();
            break;
          }
          case(WAIT_PAIR):{
            // pair has been accepted
            currPair = tmp.src;
            // TODO: rethink, 
            // maybe there should be a state of gun acquisition
            currentState.changeState(ROLLING);
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
        nackVec.data.push_back(tmp.src);
        nackVec.lock();
        nackVec.signal();
        nackVec.unlock();
        break;
      }
      case RELEASE : {
        break;
      }
      case ROLL : {
        break;
      }
      case END : {
        if(currentState >= WAIT_END){
          // send END to next in the ring
          sendPacket(&tmp,(rank+1)%size,END);
          break;
        }
        sendPacket(&tmp, tmp.value, NACK);
        break;
      }
    }
  }
  pthread_exit(NULL);
}
/* communication thread */

