#include <common.hpp>
#include <util.hpp>

void check_thread_support(int provided)
{
  debug("THREAD SUPPORT: chcemy %d. Co otrzymamy?\n", provided);
  switch (provided) {
    case MPI_THREAD_SINGLE: 
      printf("Brak wsparcia dla wątków, kończę\n");
      /* Nie ma co, trzeba wychodzić */
	    fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
	    MPI_Finalize();
	    exit(-1);
	    break;
    case MPI_THREAD_FUNNELED: 
      printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
	    break;
    case MPI_THREAD_SERIALIZED: 
      /* Potrzebne zamki wokół wywołań biblioteki MPI */
      printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
	    break;
    case MPI_THREAD_MULTIPLE: debug("Pełne wsparcie dla wątków\n"); /* tego chcemy. Wszystkie inne powodują problemy */
	    break;
    default: printf("Nikt nic nie wie\n");
  }
}
