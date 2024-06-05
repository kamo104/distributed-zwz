#pragma once

#include <common.hpp>

/* communication thread */
class CommThread{
private:
  bool begun = false;
  static void* start(void*);
  pthread_t thread_id;
public:
  void begin(){
    pthread_create( &thread_id, NULL, start , 0);
    begun = true;
  }

  ~CommThread(){
    if(!begun){
      return;
    }
    println("czekam na wątek \"komunikacyjny\"\n" );
    pthread_join(thread_id,NULL);
  }
};
/* communication thread */
