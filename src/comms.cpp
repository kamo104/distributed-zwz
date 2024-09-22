#include <common.hpp>
#include <comms.hpp>

#include <cstdint>
#include <string>

/* communication thread */
void* CommThread::start(void* ptr){
  packet_t tmp;
  MPI_Status status;
  while(currentState != FINISHED && 
    currentCycle != cyclesNum-1)
  {
    MPI_Recv(&tmp, sizeof(packet_t), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
  	// update Lamport Clock
  	clk.update(tmp.timestamp);
    // debug("recv pktDump:\n%s", packetDump(tmp).c_str());
    debug("otrzymałem %s od %d", toString(tmp.type).c_str(), tmp.src)
    switch(tmp.type){
      case ROLE : {
        roleQueue.push(tmp);
        // FIX: compare my waitQueue entry not current clock
        if(roleQueue.findPkt(rank) == NULL ||
           compare(tmp,*roleQueue.findPkt(rank))){
          sendPacket(&tmp, tmp.src, ROLE_ACK);
          break;
        }
        sendPacket(&tmp,tmp.src, ROLE_NACK);
        break;
      }
      case ROLE_ACK : {
        roleCounter.incrACK();
        break;
      }
      case ROLE_NACK : {
        roleCounter.incrNACK(tmp.src);
		    break;
      }
      case PAIR: {
        if (currentState != WAIT_PAIR){
          sendPacket(NULL, tmp.src, PAIR_NACK);
          break;
        }
        sendPacket(NULL, tmp.src, PAIR_ACK);
        currentState.changeState(ROLLING);
        break;
      }
      case PAIR_ACK: {
        if(killer) currentState.changeState(WAIT_GUN);
        else currentState.changeState(ROLLING);
        break;
      }
      case PAIR_NACK: {
        usleep(100);
        sendPacket(NULL, currPair, PAIR);
        break;
      }
      case GUN: {
        gunQueue.push(tmp);
        if(gunQueue.findPkt(rank) == NULL || 
           compare(tmp,*gunQueue.findPkt(rank))){
          sendPacket(&tmp, tmp.src, GUN_ACK);
          break;
        }
        sendPacket(&tmp,tmp.src, GUN_NACK);
        break;
      }
      case GUN_ACK: {
        gunCounter.incrACK();
        break;
      }
      case GUN_NACK: {
        gunCounter.incrNACK(tmp.src);
        break;
      }
      case RELEASE : {
        gunCounter.convert(tmp.src);
        gunQueue.remove(tmp.src);
		    break;
      }
      case ROLL : {
    		int pairRollVal = tmp.value;
    		if(rollVal == -1){
    			rollVal = random()%INT32_MAX;
    			tmp.value = rollVal;
    			sendPacket(&tmp, tmp.src, ROLL);
    		}
    		if(rollVal < pairRollVal) winAmount++;
    		currentState.changeState(WAIT_END);
        break;
      }
      case END : {
        currentState.lock();
        if(currentState >= WAIT_END){
          // send END to next in the ring
          debug("przesyłam END dalej");
          sendPacket(&tmp,(rank+1)%size, END);
          currentState.unlock();
          break;
        }
        // debug("odpowiadam NACK z powodu stanu");
        // sendPacket(&tmp, tmp.value, NACK);
        // currentState.unlock();
        break;
      }
    }
  }
  pthread_exit(NULL);
}
/* communication thread */

