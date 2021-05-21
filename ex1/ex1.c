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

int getChunk(FILE *file, char* buff, int chunkSize);

void printProcessingResults(int ntexts, char *files[]);

void processDataString(unsigned char * data);

//COMPILE AND EXECUTE
// mpicc -Wall -o ex1 ex1.c helperfuncs.c
// mpiexec -n 4 ./ex1 texts/text0.txt texts/text1.txt texts/text2.txt texts/text3.txt texts/text4.txt


/*Struct that will save the reults of the processing*/
struct PartialInfo finalInfo[10];

int main(int argc, char *argv[]){

    int rank, totProc;
    unsigned int whatToDo;                                                                                 /* command */

    //data
    int numberOfFiles = argc-1;
    FILE *file;
    static int chunkSize = 1050;

    //Initialize a struct for each text
    for (int i = 0; i<numberOfFiles; i++){
        finalInfo[i].nwords = 0;
        finalInfo[i].textInd = i;
        finalInfo[i].data = (int**)malloc(sizeof(int*));
        finalInfo[i].data[0] = (int*)malloc(sizeof(int));
        finalInfo[i].data[0][0] = 0;
        finalInfo[i].rows = 1;
    }

    //MPI
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
    Send a chunk of data to a worker process for processing
    Receive the results of the processing
    Assemble the partial info received with the final info
    */
    if (rank == 0)
    { 
        /* dispatcher process
        it is the first process of the group */

        char buff[chunkSize];
        for (int i = 0; i < numberOfFiles; i++){    //for all files

            //open file
            file = fopen(argv[i+1], "r"); 
            if (file == NULL)
            {
                printf("\nUnable to open file.\n");
                exit(EXIT_FAILURE);
            }

            int fileFinished = 0;
            while(!fileFinished){   //while file is not done
                
                int nProcesses=0;   //number of processes that got chunks

                /*
                Send a chunk of data to each worker process for processing
                */
                for (int nProc = 1 ; nProc < totProc ; nProc++){   //for all processes
                    
                    if (fileFinished)
                        break;
                    nProcesses++;
                    
                    fileFinished = getChunk(file, buff, chunkSize);

                    /*Warn workers that the work is not over and give them the buffer*/
                    whatToDo = WORKTODO;
                    MPI_Send (&whatToDo, 1, MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD);
                    MPI_Send (buff, chunkSize, MPI_CHAR, nProc, 0, MPI_COMM_WORLD);
                }

                /*
                Receive data From each process
                Assemble the partial data received with the data that was stored in the final info
                */
                for (int nProc = 1 ; nProc < nProcesses+1 ; nProc++){   //for all processes
                    int nWords;
                    int rows;
                    
                    /*Receive number of words and number of rows*/
                    MPI_Recv (&nWords,1,MPI_INT,nProc,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
                    MPI_Recv (&rows,1,MPI_INT,nProc,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);

                    /*resize matrix if necessary since this blob of data can have bigger words*/
                    finalInfo[i].data = prepareMatrix(&(finalInfo[i].rows), rows-1, finalInfo[i].data);
                    /*Sum total number of words*/
                    finalInfo[i].nwords += nWords;
                    
                    for (int row = 1; row < rows; row++){   //row represents number of chars in word (row 2 = words with 2 chars)
                        int data[row+2];
                        /*Receive row data*/
                        MPI_Recv (data,row+2,MPI_INT,nProc,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);

                        for (int col = 0; col < row + 1; col++){   //col represents number of consonants (if row = 2, col will get the value of 0, 1 and 2)
                            finalInfo[i].data[row][col] += data[col];
                        }
                        finalInfo[i].data[row][row+1] += data[row+1]; //the number of words is stored in the last cell of the array
                    }

                }
            }
        }  
        
        /*All texts are over, Dismiss the worker processes*/
        whatToDo = NOMOREWORK;
        for (int nProc = 1 ; nProc < totProc ; nProc++)   //for all processes
            MPI_Send (&whatToDo, 1, MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD);    //dismiss
        
        //Print results
        printProcessingResults(numberOfFiles, argv);

    }

    /*
    Worker Process
    
    Receive Data chunk and Count words, consonants per length, etc
    Send the results to the dispatcher
    */
    else {  

        char buff[1050];
        struct PartialInfo partialInfo;

        while (1){
            
            /*If message received is NOMOREWORK THEN PROCESS DIES*/
            MPI_Recv (&whatToDo, 1, MPI_UNSIGNED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);     
            if (whatToDo == NOMOREWORK) 
                break;

            MPI_Recv (buff,chunkSize,MPI_CHAR,0,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);    //receive buffer

            int i = 0;
            char ch;

            //init flags
            int consonants = 0;
            int inword = 0;
            int numchars = 0;
            
            //make triangular matrix
            partialInfo.nwords = 0;
            partialInfo.data = (int**)malloc(sizeof(int*));
            partialInfo.data[0] = (int*)malloc(sizeof(int));
            partialInfo.data[0][0] = 0;
            partialInfo.rows = 1;

            processDataString(buff);

            /*Count words consonants etc*/
            while (i < strlen(buff)){   //go through the chars in the buffer

                ch = buff[i];
                i++;
                
                ch = tolower(ch);   //lower case

                if ((int)ch < 0 || (int)ch > 127)   //remove non ascii
                    continue;

                if (ch == '\'' || ch == '`'){    //apostrhope
                    continue;
                }

                int isStopChar = stopChars(ch);

                if (isStopChar && inword){ //word already started and end of word
                    partialInfo.nwords++;
                    partialInfo.data = prepareMatrix(&partialInfo.rows, numchars, partialInfo.data);
                    partialInfo.data[numchars][consonants]++;   //add consonant count
                    partialInfo.data[numchars][numchars+1]++;   //add word count 

                    consonants = 0;
                    numchars = 0;
                    inword = 0;
                }

                if (!isStopChar){
                    inword = 1;
                    numchars++;
                    if (isConsonant(ch))
                        consonants++;
                }
            }
            if (inword == 1){ //last word
                partialInfo.data = prepareMatrix(&partialInfo.rows, numchars, partialInfo.data);
                partialInfo.data[numchars][consonants]++; 
                partialInfo.data[numchars][numchars+1]++;
                partialInfo.nwords++;
            }

            /*Send The results to the rank 0
            Send Number of words
            Send Number of rows in the structure
            Send each row of the structure*/
            MPI_Send (&partialInfo.nwords, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);    //send number of words
            MPI_Send (&partialInfo.rows, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);    //send number of rows

            for (int row = 1; row < partialInfo.rows; row++)   //row represents number of chars in word (row 2 = words with 2 chars)
                MPI_Send (partialInfo.data[row],row+2,MPI_INT,0,0,MPI_COMM_WORLD);

            free(partialInfo.data);
        }

    }

    MPI_Finalize ();
    return EXIT_SUCCESS;
}

/**
 *  \brief process a data chunk removing the accentuation and bad characters.
 *
 *  \param data buffer containing the data
 */

void processDataString(unsigned char * data)
{

    wint_t c;

    unsigned long data_len = strlen((const char*)data);
    unsigned char temp[1050+1];
    int i=0;
    int j=0;
    while(i<data_len){
        int inci = 1;
        int incj = 1;
        c = data[i];
        /* if the character is lower then 128 ASCII numbers*/
        if(c<=128){
            temp[j] = data[i];
        }else{
            /* In case the of a UTF-8 character && refering to a special character letter*/
            if(c == 0xc3){
                wint_t c2 = data[i+1];
                if((c2 >= 0x80 && c2 <= 0x86) || (c2 >= 0xa0 && c2 <= 0xa6)){
                    temp[j] = 'A';
                }else if((c2 >= 0xa8 && c2 <= 0xab) || (c2 >= 0x88 && c2 <= 0x8b)){
                    temp[j] = 'E';
                }else if((c2 >= 0xac && c2 <= 0xaf) || (c2 >= 0x8c && c2 <= 0x8f)){
                    temp[j] = 'I';
                }else if((c2 >= 0xb2 && c2 <= 0xb6) || (c2 >= 0x92 && c2 <= 0x96)){
                    temp[j] = 'O';
                }else if((c2 >= 0xb9 && c2 <= 0xba) || (c2 >= 0x99 && c2 <= 0x9c)){
                    temp[j] = 'U';
                }else if(c2 == 0xa7 || c2 == 0x87){
                    temp[j] = 'C';
                }else{
                    incj = 0;
                }
                inci = 2;   
            }else if(c == 0xc2){
                temp[j] = 0x20;
                inci = 2;
            }else if(c == 0xe2){
                wint_t c2 = data[i+1];
                wint_t c3 = data[i+2];
                if((c2 == 0x80 && c3 == 0x93) || (c2 == 0x80 && c3 == 0xa6)){
                    temp[j] = 0x20;
                }else if((c2 == 0x80 && c3 == 0x98) || (c2 == 0x80 && c3 == 0x99)){
                    temp[j] = 0x27;
                }else{
                    incj = 0;
                }
                inci = 3;
            }
        }
        temp[j] = temp[j] > 96 ? temp[j]-32 : temp[j];
        i += inci;
        j += incj;
    }
    memcpy(data, temp, j);
    data[j]='\0';
}

/**
 *  \brief Print function.
 *
 *  Print the results stored in the FinalInfo structure.
 *
 *  \param ntexts number of texts
 *  \param files array with file names
 */

void printProcessingResults(int ntexts, char *files[]){

    for (int f = 0; f < ntexts; f++){
        printf("Statistics for %s\n", files[f+1]);
        printf("Total words = %d\n", finalInfo[f].nwords);
        
        //LINE 1
        printf("  ");
        for (int i=1; i<finalInfo[f].rows; i++){
            printf("%6d", i);
        }
        printf("\n");
        
        //LINE 2
        printf("  ");
        for (int i=1; i<finalInfo[f].rows; i++){
            printf("%6d", finalInfo[f].data[i][i+1]);
        }
        printf("\n");

        //LINE 3
        printf("  ");
        for (int i=1; i<finalInfo[f].rows; i++){
            printf("%6.2f", 100 * (float)finalInfo[f].data[i][i+1] / (float)finalInfo[f].nwords);
        }
        printf("\n");

        //---------
        for (int col = 0; col<finalInfo[f].rows; col++){
            printf("%2d", col);
            for (int row = 1; row < finalInfo[f].rows; row++){
                if (col > row){
                    printf("      ");
                }
                else{
                    if(finalInfo[f].data[row][row+1] == 0)
                        printf("%6.1f", 0.0);
                    else
                        printf("%6.1f", 100 * (float)finalInfo[f].data[row][col] / (float)finalInfo[f].data[row][row+1]);

                }
            }
            printf("\n");
        }
    }
}

/**
 *  \brief Reads a data chunk from a file
 *
 *  Reads a chunk of data without cutting utf8 chars
 *
 *  \param file file being read
 *  \param buff buffer were the chunk will be stored
 *  \param chunkSize size of the chunk to be read
 */
int getChunk(FILE *file, char* buff, int chunkSize){
    char character;
    int fileFinished = 0;
    memset(buff, 0, sizeof buff);   //clear buffer

    while(1) {  //fill buffer

        character = fgetc(file);

        if (character == EOF){  //File has ended
            fclose(file);
            fileFinished = 1;
            break;
        }

        strcat(buff, &character);
        for (int i = 0; i < numberOfBytesInChar((unsigned char)character) - 1; i++) {
            character = fgetc(file);
            strcat(buff, &character);
        }

        if (strlen(buff)>chunkSize-50 && stopChars(buff[strlen(buff) - 1])){   //Fill buffer with complete words
            break;
        }

    }
    return fileFinished;

}
