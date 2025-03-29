#include <iostream>
#define SIZE 4

void matmul(int X[SIZE][SIZE], int Y[SIZE][SIZE], int Z[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            Z[i][j] = 0;
            for (int k = 0; k < SIZE; k++) {
                Z[i][j] += X[i][k] * Y[k][j];
            }
        }
    }
}

int main() {
    int X[SIZE][SIZE] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    int Y[SIZE][SIZE] = {{2,2,2,2},{2,2,2,2},{2,2,2,2},{2,2,2,2}};
    int Z[SIZE][SIZE];
    
    matmul(X, Y, Z);
    return 0;
}
