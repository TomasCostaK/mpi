#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <mpi.h>
#include "helperfuncs.h"
#include <wchar.h>

# define  WORKTODO       1
# define  NOMOREWORK     0

/**
 *  \brief Reads the first line from a file and returns its signal_size
 *
 *  \param file file being read
 *  \param chunkSize size of the signal_size to be read
 */
void getSignalSize(FILE *file, int * chunkSize){
    fread(chunkSize, sizeof(int), 1, file);
    return;
}

/**
 *  \brief Reads the first line from a file and returns its signal_size
 *
 *  \param file file being read
 *  \param chunkSize size of the signal_size to be read
 */
double computeValue(int n, double * x, double * y, int point){
    double result = 0;
    // Circular cross

    for (int k=0; k<n; k++){
        result += x[k] * y[(point+k) % n];
    }

    return (double) result;
}

/*Struct that will save the reults of the processing*/
struct PartialInfo finalInfo[10];

int main(int argc, char *argv[])
{   
    // Initialize MPI vars
    int rank, totProc;
    unsigned int whatToDo;  
    // Default size, this will change later as we read files                                                                               /* command */
    static int chunkSize = 0;

    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &totProc);

    /*This program only works when there are more than 1 processes*/
    if (totProc < 2) {
        fprintf(stderr,"Requires at least two processes.\n");
        MPI_Finalize ();
        return EXIT_FAILURE;
    }

    /*
    Dispatcher process
    Process will:
    
    read the files
    Send a point of data to a worker process for processing
    Receive the results of the processing
    Assemble the partial info received with the final info
    */
    if (rank == 0)
    { 
        /* dispatcher process
        it is the first process of the group */

        //data
        int numberOfFiles = argc-1;
        FILE *file;
        double t0, t1; 

        t0 = ((double) clock ()) / CLOCKS_PER_SEC;  //start elapsed time

        /*start reading text files*/
        for (int i = 0; i < numberOfFiles; i++){    //for all files

            //open file
            file = fopen(argv[i+1], "ab+"); 
            if (file == NULL)
            {
                printf("\nUnable to open file.\n");
                exit(EXIT_FAILURE);
            }

            // Get signal_size
            getSignalSize(file, &chunkSize);    
            
            double x[chunkSize], y[chunkSize], xy[chunkSize], xy_true[chunkSize];
            int currPoint = 0;

            // Reading the values of the given file, so we can then send these values in the processing
            fread(&x, sizeof(double [chunkSize]), 1, file);    
            fread(&y, sizeof(double [chunkSize]), 1, file);    
            fread(&xy_true, sizeof(double [chunkSize]), 1, file);    
            fread(&xy, sizeof(double [chunkSize]), 1, file); 

             //Allocate structure for each file
            finalInfo[i].x = x;
            finalInfo[i].y = y;
            finalInfo[i].xy_true = xy_true;
            finalInfo[i].xy = (double*)malloc(sizeof(double [chunkSize]));

            int endedText = 0;
            while(endedText == 0){   //while file is not done
                
                int nProcesses=0;   //number of processes that got chunks

                
                for (int nProc = 1 ; nProc < totProc ; nProc++){   //for all processes
                    
                    if (currPoint == chunkSize){
                        endedText = 1;
                        break;
                    }
                    nProcesses++;
                    
                    //Warn workers that the work is not over and give them the point to process
                    whatToDo = WORKTODO;
                    MPI_Send (&whatToDo, 1, MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD);
                    MPI_Send (&chunkSize, 1, MPI_INT, nProc, 0, MPI_COMM_WORLD);
                    MPI_Send (&currPoint, 1, MPI_INT, nProc, 0, MPI_COMM_WORLD);
                    MPI_Send (&finalInfo[i].x[0], chunkSize, MPI_DOUBLE, nProc, 0, MPI_COMM_WORLD);
                    MPI_Send (&finalInfo[i].y[0], chunkSize, MPI_DOUBLE, nProc, 0, MPI_COMM_WORLD);
                    currPoint += 1;
                } 

                /*
                Receive data From each process
                Assemble the partial data received with the data that was stored in the final info
                */
                for (int nProc = 1 ; nProc < nProcesses+1 ; nProc++){   //for all processes
                    double point_value;
                    int pointIdx;

                    MPI_Recv (&pointIdx,1,MPI_INT,nProc,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
                    /*Receive pointIdx data*/
                    MPI_Recv (&point_value,1,MPI_DOUBLE,nProc,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
                    finalInfo[i].xy[pointIdx] = point_value;
                    //printf("Dispatcher received point XY[%d]=%f, XY_TRUE=: %f\n", pointIdx ,point_value, finalInfo->xy_true[pointIdx]);
                }
            }
        }
        
        /*All texts are over, Dismiss the worker processes */
        whatToDo = NOMOREWORK;
        for (int nProc = 1 ; nProc < totProc ; nProc++)   //for all processes
            MPI_Send (&whatToDo, 1, MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD);    //dismiss
        
        //Print results
        t1 = ((double) clock ()) / CLOCKS_PER_SEC;
        // printProcessingResults(numberOfFiles, argv);
        printf ("\nElapsed time = %.6f s\n", t1 - t0);

    }

    /*
    Worker Process
    
    Receive Data chunk and Count words, consonants per length, etc
    Send the results to the dispatcher
    */
    else {  

        int currPoint;
        int signalSize;
        double processed_point;

        while (1){
            MPI_Recv (&whatToDo, 1, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);     
            if (whatToDo == NOMOREWORK) 
                break;
            // Receives the point for processing
            MPI_Recv (&signalSize,1, MPI_INT, 0, 0, MPI_COMM_WORLD,MPI_STATUS_IGNORE);    //receive buffer
            MPI_Recv (&currPoint,1, MPI_INT, 0, 0, MPI_COMM_WORLD,MPI_STATUS_IGNORE);    //receive buffer
            double x[signalSize], y[signalSize];
            MPI_Recv (&x, signalSize, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD,MPI_STATUS_IGNORE);    //receive buffer
            MPI_Recv (&y, signalSize, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD,MPI_STATUS_IGNORE);    //receive buffer
            //printf("Worker %d received, X[%d] = %f\n", rank ,currPoint ,x[currPoint]);

            // Process the given point
            processed_point = computeValue(signalSize, x, y, currPoint);
            //printf("Processed point: %.4f\n\n", processed_point);

            MPI_Send (&currPoint, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            MPI_Send (&processed_point, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);    //send processed point
        }
    }

    MPI_Finalize ();
    return EXIT_SUCCESS;
}