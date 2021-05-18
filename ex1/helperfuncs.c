#include <stdlib.h>

int** allocTriMatrix(int oldrows, int newrows, int ** m){

    m = (int**)realloc(m, newrows * sizeof(int*));
    for(int i = oldrows; i < newrows; i++){
        int ncols = i + 2;  //number of chars + 0 consonant case + number of words
        m[i] = (int*)malloc(ncols * sizeof(int));
        for (int j = 0; j < ncols; j++){
            m[i][j] = 0;
        }
    }
    return m;
}

int** prepareMatrix(int *currentRows, int row, int ** m){
    //Check if matric needs more space
    //row simbolizes the number of char in the word that is going to be addded
    row++;  //since row 0 exists (words with 0 char), need to do row++
    if (row > *currentRows){
        m = allocTriMatrix(*currentRows, row, m);
        *currentRows = row;
    }

    return m;

}

int isInList(char val, char arr[], int len){
    int i;
    for(i = 0; i < len; i++)
    {
        if(arr[i] == val)
            return 1;
    }
    return 0;
}

int stopChars(char val)
{
    char arr[16] = {' ', '\t', '\n', '\0', '.', ',', ':', ';', '?', '!', '-', '"', '(', ')', '[',']'};
    return isInList(val, arr, 16);
}

int isConsonant(char val)
{
    char arr[21] = {'b', 'c', 'd', 'f', 'g','h', 'j', 'k', 'l', 'm','n', 'p', 'q', 'r', 's', 't', 'v', 'w', 'x', 'y', 'z'};
    return isInList(val, arr, 21);
}

int numberOfBytesInChar(unsigned char val) {
    if (val < 128) {
        return 1;
    } else if (val < 224) {
        return 2;
    } else if (val < 240) {
        return 3;
    } else {
        return 4;
    }
}