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
    // debug("recv pktDump:\n%s", packetDump(tmp).c_str());
  	// update Lamport Clock
  	clk.update(tmp.timestamp);
  	debug("otrzymałem: %s",toString(tmp.type).c_str());
    switch(tmp.type){
      case ACK : {
        switch(currentState){
          case INIT : case WAIT_ROLE : case WAIT_GUN : {
            roleCounter.incrACK();
            break;
          }
          case WAIT_PAIR : {
            currentState.changeState(WAIT_GUN);
            break;
          }
        }
        break;
      }
      case NACK : {
        roleCounter.incrNACK(tmp.src);
		    break;
      }
      case REQ : {
        waitQueue.push(tmp);
        // FIX: compare my waitQueue entry not current clock
        const packet_t& myPkt = waitQueue.findPkt(rank);
        if(tmp.timestamp>myPkt.timestamp 
            && currentState <= ROLLING){
          // send nack
          debug("odsyłam NACK");
          sendPacket(&tmp, tmp.src, NACK);
          break;
        }
        // send ack
        debug("odsyłam ACK");
        sendPacket(&tmp,tmp.src,ACK);
        break;
      }
      case RELEASE : {
        roleCounter.convert(tmp.src);
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
    		// TODO: send RELEASEs to free gun if killer
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
        debug("odpowiadam NACK z powodu stanu");
        sendPacket(&tmp, tmp.value, NACK);
        currentState.unlock();
        break;
      }
      case GUN: {
        break;
      }
      case PAIR: {
        sendPacket(NULL, tmp.src, ACK);
        // TODO: check if we're ready to pair up?
        currentState.changeState(ROLLING);
        break;
      }
    }
  }
  pthread_exit(NULL);
}
/* communication thread */

