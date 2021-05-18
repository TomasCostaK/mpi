#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
   int rank;

   MPI_Init (&argc, &argv);
   MPI_Comm_rank (MPI_COMM_WORLD, &rank);
   printf ("I, process %d, am going to block waiting for the others!\n", rank);
   MPI_Barrier (MPI_COMM_WORLD);
   printf ("I, process %d, am going to proceed!\n", rank);
   MPI_Finalize ();

   return EXIT_SUCCESS;
}
