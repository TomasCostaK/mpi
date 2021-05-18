#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char *argv[])
{
   int rank;
   char *data = malloc ((strlen ("I, 0, am the leader!") + 1) * sizeof (char));
   int len = strlen ("I, 0, am the leader!") + 1;

   MPI_Init (&argc, &argv);
   MPI_Comm_rank (MPI_COMM_WORLD, &rank);
   if (rank == 0)
      { data = "I, 0, am the leader!";
        printf ("Broadcast message: %s \n", data);
      }
   MPI_Bcast(data, len, MPI_CHAR, 0, MPI_COMM_WORLD);
   printf ("I, %d, received the message: %s \n", rank, data);
   MPI_Finalize ();

   return EXIT_SUCCESS;
}
