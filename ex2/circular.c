#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>



int** process_signal(FILE *file){
    // Getting the signal size
   

    double result = 0;
    // Circular cross
    for (int i=0; i<signal_size; i++){
        for (int k=0; k<signal_size; k++){
            result += x[k] * y[(i+k) % signal_size];
        }
        xy[i] = result;
        //write to file
        fwrite(&result, sizeof(double), 1, file);
        result = 0;
    }

    fclose(file);
    return 0;
}

int main(int argc, char *argv[])
{   
    // Initialize MPI vars
    int rank, size;
    int *sendData, *recData;
    int i, signalSize;

    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);

    // Rank = 0, dispatcher work to read files
    if (rank == 0){
        for (int i = 1; i < argc; i++){
            
            printf("%s\n",argv[i]);
            FILE *file;
            file = fopen(argv[i], "ab+");     //open file

            if (file == NULL)
            {
                printf("\nUnable to open file.\n");
                exit(EXIT_FAILURE);
            }

            int signal_size;
            fread(&signal_size, sizeof(int), 1, file);    
            printf("Signal Size: %d\n", signal_size);

            recData = malloc(signal_size * sizeof(int))
            double x[signal_size], y[signal_size], xy[signal_size];

            // Reading X array of doubles
            fread(&x, sizeof(double [signal_size]), 1, file);    
            // Reading Y array of doubles
            fread(&y, sizeof(double [signal_size]), 1, file);    
        }    
    }

    // Now this work is shared between dispatcher and workers
    // read one file, then read the other and send it to the function of processing
    process_signal(file);
    
    return 0;
}

