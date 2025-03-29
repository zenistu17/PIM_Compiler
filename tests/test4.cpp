
#include <iostream>
#define ROWS 2
#define COLS 3
#define INNER 3

void multiply(int A[ROWS][INNER], int B[INNER][COLS], int C[ROWS][COLS]) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            C[i][j] = 0;
            for (int k = 0; k < INNER; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

int main() {
    int A[ROWS][INNER] = {{1,2,3},{4,5,6}};
    int B[INNER][COLS] = {{7,8},{9,10},{11,12}};
    int C[ROWS][COLS];
    
    multiply(A, B, C);
    return 0;
}
