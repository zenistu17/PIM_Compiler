
#include <iostream>
#define N 1

void multiply(int A[N][N], int B[N][N], int C[N][N]) {
    C[0][0] = A[0][0] * B[0][0];
}

int main() {
    int A[N][N] = {{5}};
    int B[N][N] = {{7}};
    int C[N][N];
    
    multiply(A, B, C);
    return 0;
}
