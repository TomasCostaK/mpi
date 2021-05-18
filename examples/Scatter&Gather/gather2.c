#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
   int rank, size;
   int *sendData, sendCount, *recData, *recCount, *disp, offset;
   int i;

   MPI_Init (&argc, &argv);
   MPI_Comm_rank (MPI_COMM_WORLD, &rank);
   MPI_Comm_size (MPI_COMM_WORLD, &size);
   recCount = malloc (size * sizeof (int));
   disp = malloc (size * sizeof (int));
   offset = 0;
   for (i = 0; i < size; i++)
   { recCount[i] = (i < 3) ? 2 * (i + 1) : 10;
     disp[i] = offset;
     offset += recCount[i];
   }
   sendData = malloc (recCount[rank] * sizeof (int));
   recData = malloc (offset * size * sizeof (int));
   printf ("Data to be sent by process %d: ", rank);
   sendCount = (rank < 3) ? 2 * (rank + 1) : 10;
   for (i = 0; i < sendCount; i++)
   { sendData[i] = 10 * rank + i;
     printf ("%2d ", sendData[i]);
   }
   printf ("\n");
   MPI_Gatherv (sendData, sendCount, MPI_INT, recData, recCount, disp, MPI_INT, 0, MPI_COMM_WORLD);
   if (rank == 0)
      { printf ("Gathered data\n");
        for (i = 0; i < offset; i++)
          printf ("%d ", recData[i]);
        printf ("\n");
      }
   MPI_Finalize ();

   return EXIT_SUCCESS;
}
