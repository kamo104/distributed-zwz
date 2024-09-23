#include <common.hpp>

/* global variables */
void init(){
  roleChannel.clear();
  gunChannel.clear();
 //  roleChannel = PacketChannel(size,size-1);
	// gunChannel = PacketChannel(size/2-1,guns-1);

	killer = false;
  rollVal = -1;
  currPair = -1;
}

bool endCondition(){
  return currentCycle == cyclesNum;
}
/* global variables */

/* packet stuff */
bool isACK(PacketType pkt){
  switch (pkt) {
    case ROLE_ACK: return true;
    case PAIR_ACK: return true;
    case GUN_ACK: return true;
    default: return false;
  }
}
bool isNACK(PacketType pkt){
  switch (pkt) {
    case ROLE_NACK: return true;
    case PAIR_NACK: return true;
    case GUN_NACK: return true;
    default: return false;
  }
}
PacketType toACK(PacketType pkt){
  switch(pkt){
    case ROLE_NACK: return ROLE_ACK;
    case PAIR_NACK: return PAIR_ACK;
    case GUN_NACK: return GUN_ACK;
    default: return END;
  }
}

std::string toString(PacketType pkt){
  switch(pkt){
    case ROLE: return "ROLE";
    case ROLE_ACK: return "ROLE_ACK";
    case ROLE_NACK: return "ROLE_NACK";
    case PAIR: return "PAIR";
    case PAIR_ACK: return "PAIR_ACK";
    case PAIR_NACK: return "PAIR_NACK";
    case GUN: return "GUN";
    case GUN_ACK: return "GUN_ACK";
    case GUN_NACK: return "GUN_NACK";
    case RELEASE: return "RELEASE";
    case ROLL: return "ROLL";
    case END: return "END";
    case SCORE: return "SCORE";
    default : return "NONE";
  }
}

bool compare(const packet_t& p1, const packet_t& p2){
  if(p1.timestamp==p2.timestamp) return p1.src < p2.src;
  return p1.timestamp < p2.timestamp;
}

std::string packetDump(const packet_t &pkt){
  std::ostringstream is;
  is << "type: " << toString(pkt.type) << "\n";
  is << "timestamp: " << pkt.timestamp << "\n";
  is << "src: " << pkt.src << "\n";
  is << "dst: " << pkt.dst << "\n";
  is << "value: " << pkt.value << "\n";
  return is.str();
}
void sendPacket(packet_t *pkt, int destination, PacketType tag, bool increment){
  int freepkt=0;
  if (pkt==0) { pkt = (packet_t*)malloc(sizeof(packet_t)); freepkt=1;}
  pkt->type = tag;
  pkt->src = rank;
  pkt->dst = destination;
  pkt->timestamp = clk.data;

  debug("wysyłam %s do %d z %d", toString(pkt->type).c_str(), pkt->dst, pkt->value)
  if(increment) clk++;

  MPI_Send(pkt,sizeof(packet_t),MPI_BYTE,destination,tag, MPI_COMM_WORLD);

  if (freepkt) free(pkt);
}
/* packet stuff */

/* state stufff */
std::string toString(StateType state){
  switch(state){
    case INIT: return "INIT";
    case WAIT_ROLE: return "WAIT_ROLE";
    case WAIT_PAIR: return "WAIT_PAIR";
    case WAIT_GUN: return "WAIT_GUN";
    case ROLLING: return "ROLLING";
    case WAIT_END: return "WAIT_END";
    case FINISHED: return "FINISHED";
    default: return "NONE";
  }
}
/* state stufff */
