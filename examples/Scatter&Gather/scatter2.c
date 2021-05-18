#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
   int rank, size;
   int *sendData, *recData, *sendCount, *disp, recCount;
   int i;

   MPI_Init (&argc, &argv);
   MPI_Comm_rank (MPI_COMM_WORLD, &rank);
   MPI_Comm_size (MPI_COMM_WORLD, &size);
   recData = malloc (10 * sizeof (int));
   sendCount = malloc (size * sizeof (int));
   disp = malloc (size * sizeof (int));
   for (i = 0; i < size; i++)
   { sendCount[i] = (i < 3) ? 2 * (i + 1) : 10;
     disp[i] = 20 * i;
   }
   if (rank == 0)
      { sendData = malloc (20 * size * sizeof (int));
        printf ("Data to be scattered\n");
        for (i = 0; i < 20 * size; i++)
        { sendData[i] = i;
          printf ("%2d ", sendData[i]);
        }
        printf ("\n");
      }
   recCount = (rank < 3) ? 2 * (rank + 1) : 10;
   MPI_Scatterv (sendData, sendCount, disp, MPI_INT, recData, recCount, MPI_INT, 0, MPI_COMM_WORLD);
   printf ("Received data by process %d: ", rank);
   for (i = 0; i < recCount; i++)
     printf ("%2d ", recData[i]);
   printf ("\n");
   MPI_Finalize ();

   return EXIT_SUCCESS;
}
