#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <mpi.h>
#include "helperfuncs.h"
#include <wchar.h>

# define  WORKTODO       1
# define  NOMOREWORK     0

struct PartialInfo finalInfo[10];

int main(int argc, char *argv[]){

    int rank, totProc;
    unsigned int whatToDo;                                                                                 /* command */

    //data
    int numberOfFiles = argc-1;
    FILE *file;
    static int chunkSize = 1050;

    for (int i = 0; i<numberOfFiles; i++){
        finalInfo[i].nwords = 0;
        finalInfo[i].textInd = i;
        finalInfo[i].data = (int**)malloc(sizeof(int*));
        finalInfo[i].data[0] = (int*)malloc(sizeof(int));
        finalInfo[i].data[0][0] = 0;
        finalInfo[i].rows = 1;
    }

    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &totProc);

    if (totProc < 2) {
        fprintf(stderr,"Requires at least two processes.\n");
        MPI_Finalize ();
        return EXIT_FAILURE;
    }

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
            printf("---------------OPENED %s-------------- \n", argv[i+1]);


            int fileFinished = 0;
            while(!fileFinished){   //while file is not done
                
                int nProcesses=0;   //number of processes that got chunks

                for (int nProc = 1 ; nProc < totProc ; nProc++){   //for all processes
                    
                    if (fileFinished)
                        break;
                    
                    nProcesses++;
                    
                    int character;
                    memset(buff, 0, sizeof buff);   //clear buffer
                    while(1) {  //fill buffer

                        character = fgetc(file);

                        if (character == EOF){  //File has ended
                            printf("Finished File\n");
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

                    whatToDo = WORKTODO;
                    MPI_Send (&whatToDo, 1, MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD);
                    MPI_Send (buff, chunkSize, MPI_CHAR, nProc, 0, MPI_COMM_WORLD);
                }

                /*Receive data from processes and assemble*/
                for (int nProc = 1 ; nProc < nProcesses+1 ; nProc++){   //for all processes
                    int nWords;
                    int rows;
                    
                    MPI_Recv (&nWords,1,MPI_INT,nProc,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
                    MPI_Recv (&rows,1,MPI_INT,nProc,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);

                    //resize matrix if necessary since this blob of data can have bigger words
                    finalInfo[i].data = prepareMatrix(&(finalInfo[i].rows), rows-1, finalInfo[i].data);
                    finalInfo[i].nwords += nWords; //total number of words
                    
                    for (int row = 1; row < rows; row++){   //row represents number of chars in word (row 2 = words with 2 chars)
                        int data[row+2];
                        MPI_Recv (data,row+2,MPI_INT,nProc,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);

                        for (int col = 0; col < row + 1; col++){   //col represents number of consonants (if row = 2, col will get the value of 0, 1 and 2)
                            finalInfo[i].data[row][col] += data[col];
                        }
                        finalInfo[i].data[row][row+1] += data[row+1]; //the number of words is stored in the last cell of the array
                    }

                }
            }
        }  
        
        whatToDo = NOMOREWORK;
        for (int nProc = 1 ; nProc < totProc ; nProc++)   //for all processes
            MPI_Send (&whatToDo, 1, MPI_UNSIGNED, nProc, 0, MPI_COMM_WORLD);    //dismiss
        
        printProcessingResults(numberOfFiles, argv);

    }
    else {  /* worker processes ----------------------------------------------------------------------------------------------
                the remainder processes of the group */


        char buff[1050];
        struct PartialInfo partialInfo;

        while (1){
            
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

            while (i < strlen(buff)){   //go through the chars in the buffer

                ch = buff[i];
                i++;
                
                ch = tolower(ch);   //lower case

                if ((int)ch < 0 || (int)ch > 127)   //remove non ascii
                    continue;

                if (ch == '\''){    //apostrhope
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
