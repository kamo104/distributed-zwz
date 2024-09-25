#include <common.hpp>
#include <comms.hpp>

#include <cstdint>
#include <string>

/* communication thread */
void* CommThread::start(void* ptr){
  packet_t tmp;
  MPI_Status status;
  while(!endCondition()){
    MPI_Recv(&tmp, sizeof(packet_t), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
  	// update Lamport Clock
  	clk.update(tmp.timestamp);
    // debug("recv pktDump:\n%s", packetDump(tmp).c_str());
    debug("otrzymałem %s od %d", toString(tmp.type).c_str(), tmp.src)
    switch(tmp.type){
      case ROLE : {
        roleChannel.lock();
        roleChannel.qpush(tmp);
        // roleChannel.dump();
        if(roleChannel.qcountFrom(rank) == 0 ||
           compare(tmp,*roleChannel.qgetFrom(rank))){
          roleChannel.unlock();
          sendPacket(&tmp, tmp.src, ROLE_ACK);
          break;
        }
        roleChannel.unlock();
        sendPacket(&tmp,tmp.src, ROLE_NACK);
        break;
      }
      case ROLE_ACK : {
        roleChannel.rpush(tmp);
        // roleChannel.dump();
        break;
      }
      case ROLE_NACK : {
        roleChannel.rpush(tmp);
        // roleChannel.dump();
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
        // usleep(100);
        // sleep(1);
        sendPacket(NULL, currPair, PAIR);
        break;
      }
      case GUN: {
        gunChannel.lock();
        gunChannel.qpush(tmp);
        // gunChannel.dump();
        if(gunChannel.qcountFrom(rank) == 0 || 
           compare(tmp,*gunChannel.qgetFrom(rank))){
          gunChannel.unlock();
          sendPacket(&tmp, tmp.src, GUN_ACK);
          break;
        }
        gunChannel.unlock();
        sendPacket(&tmp,tmp.src, GUN_NACK);
        break;
      }
      case GUN_ACK: {
        gunChannel.lock();
        gunChannel.rpush(tmp);
        // gunChannel.dump();
        gunChannel.unlock();
        break;
      }
      case GUN_NACK: {
        gunChannel.lock();
        gunChannel.rpush(tmp);
        // gunChannel.dump();
        gunChannel.unlock();
        break;
      }
      case RELEASE : {
        gunChannel.lock();
        gunChannel.rconvert(tmp.src);
        gunChannel.qremove(tmp.src);
        // gunChannel.dump();
        gunChannel.unlock();
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
			// give back the gun
			gunChannel.lock();
			clk.lock();
			for(int i=0;i<roleChannel.queue().size()/2;i++){
				if(roleChannel.queue()[i].src==rank) continue;
				sendPacket(&tmp, roleChannel.queue()[i].src, RELEASE, false);
			}
			clk++;
			clk.unlock();
			gunChannel.qremove(rank);
			gunChannel.unlock();
    		currentState.changeState(WAIT_END);
  			// TODO: move to end cycle barrier
        break;
      }
      case END : {
    		switch(tmp.value){
    			case 0: {
    				if(currentState < WAIT_END){
    					// debug("nie skończyłem jeszcze");
    					tmp.value = -1;
        			sendPacket(&tmp, highestPriorityID, END);
        			std::ostringstream os;
        			os << "bcs state: " << toString(currentState);
        			debug("%s",os.str().c_str())
      				break;
    				}
    				// initialize variables
  				  init();

  					// if I'm the initiator
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
    			case -1: {
    				// debug("ktoś jeszcze nie skończył, wznawiam sprawdzanie zakończenia")
    				// usleep(100);
    				sleep(1);
    				tmp.value = 0;
    				sendPacket(&tmp, (rank+1)%size, END);
    				break;
    			}
    			default: {
    				// debug("otrzymałem wieść o skończeniu cyklu")
    				if(tmp.value < size){
    					debug("przesyłam dalej")
    					tmp.value++;
    					sendPacket(&tmp, (rank+1)%size, END);
    				}
    				//TODO: start participating in next cycle
    				currentState.changeState(FINISHED);
    				break;
    			}
    		}
        break;
      }
  	  case SCORE: {
    		if(tmp.value == size-1){
    		  std::ostringstream os;
    		  os << "Koniec gry. Wygrał proces ID " << tmp.topId << " z wynikiem " << tmp.topScore << ".";
    		  println("%s",os.str().c_str());
    		  break;
    		}
  			if(winAmount > tmp.topScore){
  				tmp.topScore = winAmount;
  				tmp.topId = rank;
  			}
  			if(currentState < WAIT_END) tmp.value = 0;
  			else tmp.value++;
      	sendPacket(&tmp,(rank+1)%size, SCORE);
    		break;
  	  }
    }
  }
  pthread_exit(NULL);
}
/* communication thread */

