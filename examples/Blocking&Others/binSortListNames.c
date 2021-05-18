/**
 *  \file binSortListNames.c (implementation file)
 *
 *  \brief Sorting of a list of names stored in a file.
 *
 *  It sorts the list using a binary sorting algorithm which allows the sorting of parts of the original list by
 *  different processes of a group in a hierarchical way.
 *  The list of names must be supplied by the user.
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
#include <string.h>

/* General definitions */

# define  WORKTODO       1
# define  NOMOREWORK     0

/* Type definition for data transfer */

typedef struct
        { unsigned int nNames,
                       nCar;
        } FORMNAMES;

/* Allusion to internal functions */

static void split (unsigned int, unsigned int []);
static void merge (unsigned int, unsigned int [], unsigned int *, unsigned int *);
static void compAndPosSort (char *, char *);

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

   MPI_Init (&argc, &argv);
   MPI_Comm_rank (MPI_COMM_WORLD, &rank);
   MPI_Comm_size (MPI_COMM_WORLD, &totProc);

   /* processing */

   if (rank == 0)
      { /* dispatcher process
           it is the first process of the group */

        FILE *f;                                         /* pointer to the text stream associated with the file name */
        unsigned int whatToDo;                                                                            /* command */
        unsigned int nNames,                                                          /* number of names in the list */
                     nCar;                                                 /* maximum number of characters of a name */
        unsigned int minProc,                                                /* minimum number of required processes */
                     size,                                                             /* size of processing pattern */
                     n;                                                                         /* counting variable */
        unsigned int *ord;                                                                     /* processing pattern */
        char **names,                                                                                  /* name array */
             *nameSpace;                                                                            /* storage space */

        /* check running parameters and load list of names into memory */

        if (argc != 2)
           { printf ("No file name!\n");
             whatToDo = NOMOREWORK;
             for (n = 1; n < totProc; n++)
               MPI_Send (&whatToDo, 1, MPI_UNSIGNED, n, 0, MPI_COMM_WORLD);
             MPI_Finalize ();
             return EXIT_FAILURE;
           }

        if ((f = fopen (argv[1], "r")) == NULL)
           { perror ("error on file opening for reading");
             whatToDo = NOMOREWORK;
             for (n = 1; n < totProc; n++)
               MPI_Send (&whatToDo, 1, MPI_UNSIGNED, n, 0, MPI_COMM_WORLD);
             MPI_Finalize ();
             exit (EXIT_FAILURE);
           }
        fscanf (f, "%d %d", &nNames, &nCar);
        fscanf (f, "%*[^\n]");
        fscanf (f, "%*c");
        names = malloc (nNames * sizeof (char *));
        nameSpace = malloc (nNames * (nCar + 1) * sizeof (char));
        for (n = 0; n < nNames; n++, nameSpace += nCar + 1)
        { names[n] = (char *) nameSpace;
          fscanf (f, "%s", names[n]);
          fscanf (f, "%*[^\n]");
          fscanf (f, "%*c");
        }
        if (fclose (f) == EOF)
           { perror ("error on closing file");
             whatToDo = NOMOREWORK;
             for (n = 1; n < totProc; n++)
               MPI_Send (&whatToDo, 1, MPI_UNSIGNED, n, 0, MPI_COMM_WORLD);
             MPI_Finalize ();
             exit (EXIT_FAILURE);
           }

        ord = malloc (nNames * sizeof (unsigned int));
        for (n = 0; n < nNames; n++)
          ord[n] = 0;
        split (nNames, ord);
        merge (nNames, ord, &size, &minProc);
        if (minProc >= totProc)
           { printf ("Too few processes!\n");
             whatToDo = NOMOREWORK;
             for (n = 1; n < totProc; n++)
               MPI_Send (&whatToDo, 1, MPI_UNSIGNED, n, 0, MPI_COMM_WORLD);
             MPI_Finalize ();
             return EXIT_FAILURE;
           }

        printf ("Unsorted list\n");
        for (n = 0; n < nNames; n++)
        { printf ("%s\n", names[n]);
        }

        /* distribute sorting task */

        unsigned int iters,                                                                  /* number of iterations */
                     nProc;                                                                        /* process number */
        unsigned int nBase, incNBase, base, k, r, t;                   /* algorithm intrinsic and counting variables */
        FORMNAMES fmtNames;                                                                /* data transfer variable */

        fmtNames.nCar = nCar;
        base = 2;
        iters = (unsigned int) ceil (log2 ((double) nNames));
        for (k = 0, base = 2; k < iters; k++, base *= 2)
        { nBase = 0;
          nProc = 1;
          for (r = 0; r < size; r++)
          { incNBase = ord[r];
            if ((ord[r] > 1) && (ord[r] > base/2) && (ord[r] <= base))
               { whatToDo = WORKTODO;
                 MPI_Send (&whatToDo, 1, MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD);
                 fmtNames.nNames = ord[r];
                 MPI_Send (&fmtNames, sizeof (FORMNAMES), MPI_BYTE, nProc, 0, MPI_COMM_WORLD);
                 MPI_Send (names[nBase], ord[r] * (nCar + 1), MPI_CHAR, nProc, 0, MPI_COMM_WORLD);
                 nProc += 1;
               }
            nBase += incNBase;
          }
          nBase = 0;
          nProc = 1;
          for (r = 0; r < size; r++)
          { incNBase = ord[r];
            if ((ord[r] > 1) && (ord[r] > base/2) && (ord[r] <= base))
               { MPI_Recv (names[nBase], ord[r] * (nCar + 1), MPI_CHAR, nProc, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
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

        printf ("\nSorted list\n");
        for (n = 0; n < nNames; n++)
        { printf ("%s\n", names[n]);
        }
      }
      else { /* worker processes
                the remainder processes of the group */

             unsigned int whatToDo;                                                                       /* command */
             char **names,                                                                             /* name array */
                  *nameSpace;                                                                       /* storage space */
             FORMNAMES fmtNames;                                                           /* data transfer variable */
             unsigned int m, n;                                                                /* counting variables */

             while (true)
             { MPI_Recv (&whatToDo, 1, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
               if (whatToDo == NOMOREWORK) break;
               MPI_Recv (&fmtNames, sizeof (FORMNAMES), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
               names = malloc (fmtNames.nNames * sizeof (char *));
               nameSpace = malloc (fmtNames.nNames * (fmtNames.nCar + 1) * sizeof (char));
               for (n = 0; n < fmtNames.nNames; n++)
                 names[n] = (char *) (nameSpace + n * (fmtNames.nCar + 1));
               MPI_Recv (names[0], fmtNames.nNames * (fmtNames.nCar + 1), MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
               if ((fmtNames.nNames % 2) == 0)
                  { for (m = 0; m < fmtNames.nNames/2; m++)
                      for (n = 0; (m + n) < fmtNames.nNames/2; n++)
                        compAndPosSort (names[m+n], names[fmtNames.nNames/2+n]);
                  }
                  else { for (m = 0; m < fmtNames.nNames/2 + 1; m++)
                           for (n = ((m == 0) ? 1 : 0); (m + n) < fmtNames.nNames/2 + 1; n++)
                             compAndPosSort (names[m+n-1], names[fmtNames.nNames/2+n]);
                       }
               MPI_Send (names[0], fmtNames.nNames * (fmtNames.nCar + 1), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
               free (names);
               free (nameSpace);
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
 *  \param name1 first name
 *  \param name2 second name
 */

static void compAndPosSort (char name1[], char name2[])
{
  char *tmp;

  if (strlen (name1) > strlen (name2))
     tmp = malloc ((strlen (name1) + 1) * sizeof (char));
     else tmp = malloc ((strlen (name2) + 1) * sizeof (char));
  if (strcmp (name1, name2) > 0)
     { strcpy (tmp, name1);
       strcpy (name1, name2);
       strcpy (name2, tmp);
     }
  free (tmp);
}
