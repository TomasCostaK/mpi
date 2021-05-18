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
   recData = malloc (10 * sizeof (int));
   if (rank == 0)
      { sendData = malloc (10 * size * sizeof (int));
        printf ("Data to be scattered\n");
        for (i = 0; i < 10 * size; i++)
        { sendData[i] = i;
          printf ("%d ", sendData[i]);
        }
        printf ("\n");
      }
   MPI_Scatter (sendData, 10, MPI_INT, recData, 10, MPI_INT, 0, MPI_COMM_WORLD);
   printf ("Received data by process %d: ", rank);
   for (i = 0; i < 10; i++)
     printf ("%2d ", recData[i]);
   printf ("\n");
   MPI_Finalize ();

   return EXIT_SUCCESS;
}
