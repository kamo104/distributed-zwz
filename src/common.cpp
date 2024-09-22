#include <common.hpp>

/* packet stuff */
std::string toString(PacketType pkt){
  switch(pkt){
    case REQ : return "REQ";
    case ACK : return "ACK";
    case NACK : return "NACK";
    case RELEASE : return "RELEASE";
    case PAIR : return "PAIR";
    case GUN : return "GUN";
    case ROLL : return "ROLL";
    case END : return "END";
    default : return "NONE";
  }
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

  if(increment) clk++;
  debug("wysyłam: %s", toString(pkt->type).c_str())

  MPI_Send(pkt,sizeof(packet_t),MPI_BYTE,destination,tag, MPI_COMM_WORLD);

  if (freepkt) free(pkt);
}
/* packet stuff */

/* state stufff */
std::string toString(StateType state){
  switch(state){
    case INIT: return "INIT";
    case WAIT_ROLE: return "WAIT_ROLE";
    case ROLE_PICKED: return "ROLE_PICKED";
    case WAIT_PAIR: return "WAIT_PAIR";
    case WAIT_GUN: return "WAIT_GUN";
    case ROLLING: return "ROLLING";
    case WAIT_END: return "WAIT_END";
    case FINISHED: return "FINISHED";
    default: return "NONE";
  }
}
/* state stufff */
