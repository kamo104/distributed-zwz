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
  			// TODO: move to end cycle barrier
        break;
      }
      case END : {
        currentState.lock();
    		switch(tmp.value){
    			case 0: {
    				if(currentState >= WAIT_END){
    					//TODO: reinitialize variables for next cycle

    					// if you're the initiator
    					if(highestPriorityID == rank){
    						tmp.value = 1;
    						// debug("wykryto koniec cyklu; rozsyłam wieść o tym");
    						sendPacket(&tmp,(rank+1)%size, END);
    						break;
    					} 
      				// debug("przesyłam END dalej");
      				sendPacket(&tmp,(rank+1)%size, END);
    					break;
    				} 
  					// debug("nie skończyłem jeszcze");
  					tmp.value = -1;
      			sendPacket(&tmp, highestPriorityID, END);
    				break;
    			}
    			case -1: {
    				// debug("ktoś jeszcze nie skończył, wznawiam sprawdzanie zakończenia")
    				usleep(100);
    				tmp.value = 0;
    				sendPacket(&tmp, (rank+1)%size, END);
    				break;
    			}
    			default: {
    				// debug("otrzymałem wieść o skończeniu cyklu")
    				if(tmp.value < size){
    					// debug("przesyłam dalej")
    					tmp.value++;
    					sendPacket(&tmp, (rank+1)%size, END);
    				}
    				//TODO: start participating in next cycle
    			}
    		}
        currentState.unlock();
        break;
      }
  	  case SCORE: {
        currentState.lock();
    		if(tmp.value == size){
    		  std::ostringstream os;
    		  os << "Koniec rundy. Wygrał proces ID " << tmp.topId << " z wynikiem " << tmp.topScore << ".";
    		  println("%s",os.str().c_str());
    		} 
    		else {
    			if(winAmount > tmp.topScore){
    				tmp.topScore = winAmount;
    				tmp.topId = rank;
    			}
    			if(currentState < WAIT_END) tmp.value = 0;
    			else tmp.value++;
        	sendPacket(&tmp,(rank+1)%size, SCORE);
    		}
  		currentState.unlock();
  		break;
  	  }
    }
  }
  pthread_exit(NULL);
}
/* communication thread */

