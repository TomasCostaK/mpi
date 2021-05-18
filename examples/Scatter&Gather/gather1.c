#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
   int rank, size;
   int *sendData, *recData;
   int i;

   MPI_Init (&argc, &argv);
   MPI_Comm_rank (MPI_COMM_WORLD, &rank);
   MPI_Comm_size (MPI_COMM_WORLD, &size);
   sendData = malloc (10 * sizeof (int));
   recData = malloc (10 * size * sizeof (int));
   printf ("Data to be sent by process %d: ", rank);
   for (i = 0; i < 10; i++)
   { sendData[i] = 10 * rank + i;
     printf ("%2d ", sendData[i]);
   }
   printf ("\n");
   MPI_Gather (sendData, 10, MPI_INT, recData, 10, MPI_INT, 0, MPI_COMM_WORLD);
   if (rank == 0)
      { printf ("Gathered data\n");
        for (i = 0; i < 10 * size; i++)
          printf ("%d ", recData[i]);
        printf ("\n");
      }
   MPI_Finalize ();

   return EXIT_SUCCESS;
}
