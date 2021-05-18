#ifndef HELPERFUNCS_H
#define HELPERFUNCS_H

int** allocTriMatrix(int oldrows, int newrows, int ** m);

int** prepareMatrix(int *currentRows, int row, int ** m);

int isInList(char val, char arr[], int len);

int stopChars(char val);

int isConsonant(char val);

int numberOfBytesInChar(unsigned char val);

struct PartialInfo{
    int nwords;
    int ** data;
    int rows;
    int textInd;
};

#endif 