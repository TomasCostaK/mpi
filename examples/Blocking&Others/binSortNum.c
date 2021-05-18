/**
 *  \file binSortNum.c (implementation file)
 *
 *  \brief Sorting of a sequence of numbers stored in memory.
 *
 *  It sorts the sequence using a binary sorting algorithm which allows the sorting of parts of the original sequence
 *  by different processes of a group in a hierarchical way.
 *  The sequence of numbers must be supplied by the user.
 *  MPI implementation.
 *
 *  \author António Rui Borges - April 2020
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>

/* General definitions */

# define  N             27
# define  WORKTODO       1
# define  NOMOREWORK     0

/* Allusion to internal functions */

static void split (unsigned int, unsigned int []);
static void merge (unsigned int, unsigned int [], unsigned int *, unsigned int *);
static void compAndPosSort (unsigned int *, unsigned int *);

/**
 *  \brief Main function.
 *
 *  Instantiation of the processing configuration.
 *
 *  \param argc number of words of the command line
 *  \param argv list of words of the command line
 *
 *  \return status of operation
 */

int main (int argc, char *argv[])
{
   int rank,                                                                     /* number of processes in the group */
       totProc;                                                                                        /* group size */

   /* get processing configuration */

   unsigned int whatToDo;                                                                                 /* command */
   unsigned int ord[N];                                                                        /* processing pattern */
   unsigned int minProc,                                                     /* minimum number of required processes */
                size,                                                                  /* size of processing pattern */
                n;                                                                              /* counting variable */

   MPI_Init (&argc, &argv);
   MPI_Comm_rank (MPI_COMM_WORLD, &rank);
   MPI_Comm_size (MPI_COMM_WORLD, &totProc);

   for (n = 0; n < N; n++)
     ord[n] = 0;
   split (N, ord);
   merge (N, ord, &size, &minProc);

   /* check running parameters */

   if (minProc >= totProc)
      { if (rank == 0) printf ("Too few processes!\n");
        MPI_Finalize ();
        return EXIT_FAILURE;
      }

   /* processing */

   if (rank == 0)
      { /* dispatcher process
           it is the first process of the group */

        /* generate number sequence */

        unsigned int val[N];

        srandom (getpid ());
        for (n = 0; n < N; n++)
          val[n] = ((double) rand () / RAND_MAX) * 1000;

        printf ("Unsorted sequence\n");
        for (n = 0; n < N; n++)
          printf ("%4d", val[n]);
        printf ("\n");

        /* distribute sorting task */

        unsigned int iters,                                                                  /* number of iterations */
                     nProc;                                                                        /* process number */
        unsigned int nBase, incNBase, base, k, r, t;                   /* algorithm intrinsic and counting variables */

        base = 2;
        iters = (unsigned int) ceil (log2 ((double) N));
        for (k = 0, base = 2; k < iters; k++, base *= 2)
        { nBase = 0;
          nProc = 1;
          for (r = 0; r < size; r++)
          { incNBase = ord[r];
            if ((ord[r] > 1) && (ord[r] > base/2) && (ord[r] <= base))
               { whatToDo = WORKTODO;
                 MPI_Send (&whatToDo, 1, MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD);
                 MPI_Send (&ord[r], 1, MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD);
                 MPI_Send (val + nBase, ord[r], MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD);
                 nProc += 1;
               }
            nBase += incNBase;
          }
          nBase = 0;
          nProc = 1;
          for (r = 0; r < size; r++)
          { incNBase = ord[r];
            if ((ord[r] > 1) && (ord[r] > base/2) && (ord[r] <= base))
               { MPI_Recv (val + nBase, ord[r], MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                 nProc += 1;
                 t = r - base/2;
                 while (t < (r - base/4))
                 { if ((r > (base/2-1)) && (ord[t] != 0) && (ord[t] <= ord[r]))
                      { ord[r] += ord[t];
                        ord[t] = 0;
                        break;
                      }
                   t += 1;
                  }
               }
            nBase += incNBase;
          }
        }

        /* dismiss worker processes */

        whatToDo = NOMOREWORK;
        for (r = 1; r < totProc; r++)
          MPI_Send (&whatToDo, 1, MPI_UNSIGNED, r, 0, MPI_COMM_WORLD);

        printf ("\nSorted sequence\n");
        for (n = 0; n < N; n++)
          printf ("%4d", val[n]);
        printf ("\n");
      }
      else {  /* worker processes
                the remainder processes of the group */

             unsigned int nElem;                                                       /* number of elements to sort */
             unsigned int *valPart;                                                                 /* storage space */
             unsigned int m, n;                                                                /* counting variables */

             while (true)
             { MPI_Recv (&whatToDo, 1, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
               if (whatToDo == NOMOREWORK) break;
               MPI_Recv (&nElem, 1, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
               valPart = malloc (nElem * sizeof (unsigned int));
               MPI_Recv (valPart, nElem, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
               if ((nElem % 2) == 0)
                  { for (m = 0; m < nElem/2; m++)
                      for (n = 0; (m + n) < nElem/2; n++)
                        compAndPosSort (valPart + m + n, valPart + nElem/2 + n);
                  }
                  else { for (m = 0; m < nElem/2 + 1; m++)
                           for (n = ((m == 0) ? 1 : 0); (m + n) < nElem/2 + 1; n++)
                             compAndPosSort (valPart + m + n - 1, valPart + nElem/2 + n);
                       }
               MPI_Send (valPart, nElem, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD);
               free (valPart);
             }
           }

   MPI_Finalize ();
   return EXIT_SUCCESS;
}

/**
 *  \brief Split function.
 *
 *  Stage 1 of establishing the processing pattern.
 *
 *  \param n number of elements to sort
 *  \param ord processing pattern
 */

static void split (unsigned int n, unsigned int ord[])
{
  if (n > 2)
     { ord[0] = n/2;
       ord[n/2] = n - n/2;
       split (n/2, ord);
       split (n - n/2, ord + n/2);
     }
     else if (n == 2)
             ord[0] = 2;
}

/**
 *  \brief Merge function.
 *
 *  Stage 2 of establishing the processing pattern.
 *
 *  \param n number of elements to sort
 *  \param ord processing pattern
 *  \param pSize pointer to the size of the processing pattern
 *  \param pMinProc pointer to the minimum number of required processes
 */

static void merge (unsigned int n, unsigned int ord[], unsigned int *pSize, unsigned int *pMinProc)
{
  unsigned int k, m;

  *pMinProc = 0;
  for (k = 0, m = 0; k < n; k++)
    if (ord[k] > 0)
       { ord[m] = ord[k];
         m += 1;
         if (ord[k] == 2)
            *pMinProc += 1;
       }
  *pSize = m;
}

/**
 *  \brief Compare and possible sort function.
 *
 *  If it is required, the elements are swapped.
 *
 *  \param pVal1 pointer to first value
 *  \param pVal2 pointer to second value
 */

static void compAndPosSort (unsigned int *pVal1, unsigned int *pVal2)
{
  unsigned int tmp;

  if (*pVal1 > *pVal2)
     { tmp = *pVal1;
       *pVal1 = *pVal2;
       *pVal2 = tmp;
     }
}
