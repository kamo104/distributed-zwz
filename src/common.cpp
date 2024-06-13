#include <common.hpp>

/* packet stuff */
void sendPacket(packet_t *pkt, int destination, PacketType tag, bool increment=true){
  int freepkt=0;
  if (pkt==0) { pkt = (packet_t*)malloc(sizeof(packet_t)); freepkt=1;}
  pkt->type = tag;
  pkt->src = rank;
  pkt->dst = destination;
  pkt->timestamp = clk.data;

  if(increment) clk++;

  MPI_Send(&pkt,sizeof(packet_t),MPI_BYTE,destination,tag, MPI_COMM_WORLD);

  // debug("Wysyłam %s do %d\n", tag2string( tag), destination);
  if (freepkt) free(pkt);
}
/* packet stuff */
