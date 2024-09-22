#include <common.hpp>
#include <comms.hpp>

#include <cstdint>

/* communication thread */
void* CommThread::start(void* ptr){
  packet_t tmp;
  MPI_Status status;
  while(currentState != FINISHED && 
    currentCycle != cyclesNum-1)
  {
    MPI_Recv(&tmp, 1, MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
  	// update Lamport Clock
  	clk.update(tmp.timestamp);
    switch(tmp.type){
      case ACK : {
        debug("otrzymałem ACK");
        currentState.lock();
        switch(currentState){
          case INIT : case WAIT_ROLE : case WAIT_GUN : {
            cnt.incrACK();
            break;
          }
          case WAIT_PAIR : {
            debug("przechodzę do stanu WAIT_GUN");
            currentState.changeState(WAIT_GUN);
            break;
          }
          default : {
            debug("jestem w stanie innym niż {INIT, WAIT_ROLE, WAIT_GUN, WAIT_PAIR}, a otrzymałem ACK, WTF");
            break;
          }
        }
        currentState.unlock();
        break;
      }
      case REQ : {
        debug("dostałem REQ");
        waitQueue.push(tmp);
        if(tmp.timestamp<clk.data 
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
      case NACK : {
        debug("otrzymałem NACK");
        cnt.incrNACK(tmp.src);
		    break;
      }
      case RELEASE : {
        debug("otrzymałem RELEASE");
        cnt.convert(tmp.src);
		    break;
      }
      case ROLL : {
        debug("otrzymałem ROLL");
    		int pairRollVal = tmp.value;
    		if(rollVal == -1){
    			rollVal = random()%INT32_MAX;
    			tmp.value = rollVal;
    			sendPacket(&tmp, currPair, ROLL);
    		}
    		if(rollVal < pairRollVal) winAmount++;
    		// TODO: send RELEASEs to free gun if killer
			// TODO: move to end cycle barrier
        break;
      }
      case END : {
        debug("otrzymałem END");
        currentState.lock();
		switch(tmp.value){
			case 0: {
				if(currentState >= WAIT_END){
					//TODO: reinitialize variables for next cycle

					// if you're the initiator
					if(highestPriorityID == rank){
						tmp.value = 1;
						debug("wykryto koniec cyklu; rozsyłam wieść o tym");
						sendPacket(&tmp,(rank+1)%size, END);
					} else {
						// send END to next in the ring
        				debug("przesyłam END dalej");
        				sendPacket(&tmp,(rank+1)%size, END);
					}
				} else {
					debug("nie skończyłem jeszcze");
					tmp.value = -1;
        			sendPacket(&tmp, highestPriorityID, END);
				}
				break;
			}
			case -1: {
				debug("ktoś jeszcze nie skończył, wznawiam sprawdzanie zakończenia")
				tmp.value = 0;
				sendPacket(&tmp, (rank+1)%size, END);
				break;
			}
			default: {
				debug("otrzymałem wieść o skończeniu cyklu")
				if(tmp.value < size){
					debug("przesyłam dalej")
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
		debug("otrzymałem SCORE");
        currentState.lock();
		if(tmp.value == size){
			println("Koniec rundy. Wygrał proces ID "+stoi(tmp.topId)+" z wynikiem "+stoi(tmp.topScore)+".");
		} else {
			if(winAmount > tmp.topScore){
				tmp.topScore = winAmount;
				tmp.topId = rank;
			}
			if(currentState < WAIT_END) tmp.value = 0;
			else tmp.value++;
			debug("przesyłam SCORE dalej");
        	sendPacket(&tmp,(rank+1)%size, SCORE);
		}
		currentState.unlock();
		break;
	  }
      case GUN: {
        debug("otrzymałem GUN");
        break;
      }
      case PAIR: {
        debug("otrzymałem PAIR");
        sendPacket(NULL, tmp.src, ACK);
        currentState.changeState(ROLLING);
        break;
      }
    }
  }
  pthread_exit(NULL);
}
/* communication thread */

