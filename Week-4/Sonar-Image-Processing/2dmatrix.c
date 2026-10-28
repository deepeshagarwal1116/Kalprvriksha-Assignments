#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MINVAL 2
#define MAXVAL 10

void generatematrix(int **mat, int *data, int n)
{
    for (int k = 0; k < n * n; k++) {
         *(data + k) = rand() % 256;   // random value (0-255)
       // scanf("%d",data+k);
        *(mat + k) = data + k;        // cell k points to its value
    }
}

void printmatrix(int **mat, int n)
{
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            printf("%4d", **(mat + i * n + j));
        printf("\n");
    }
}


// swap the pointers in two slots (addresses, not values)
void swapslots(int **a, int **b)
{
    int *temp = *a;
    *a = *b;
    *b = temp;
}

// rotate 90° clockwise first transpose and then reverse row by row
void rotatematrix(int **mat, int n)
{
    // Step 1: transpose, swap (i,j) with (j,i)
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            swapslots(mat + i * n + j, mat + j * n + i);

    // Step 2: reverse each row, swap (i,j) with (i, n-1-j)
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n / 2; j++)
            swapslots(mat + i * n + j, mat + i * n + (n - 1 - j));
}

// 3x3 average using only two O(N) row buffers
void smoothmatrix(int **mat, int n)
{
    int *prev = malloc(n * sizeof(int));   // original values of row i-1
    int *curr = malloc(n * sizeof(int));   // original values of row i

    for (int i = 0; i < n; i++) {
        // save row i before overwriting it
        for (int j = 0; j < n; j++)
            *(curr + j) = **(mat + i * n + j);

        for (int j = 0; j < n; j++) {
            int sum = 0, count = 0;

            for (int r = i - 1; r <= i + 1; r++) {  //checking for 3*3 box at every cell
                if (r < 0 || r >= n) continue;    // skip rows outside
                    for (int c = j - 1; c <= j + 1; c++) {
                        if (c < 0 || c >= n) continue;      // skip cols outside
                        if (r == i - 1)  sum += *(prev + c);
                        else if (r == i) sum += *(curr + c);
                        else             sum += **(mat + r * n + c);   // row i+1 not changed yet
                        count++;
                    }
            }
            **(mat + i * n + j) = sum / count;
        }

        int *t = prev; prev = curr; curr = t;   // this row becomes "previous"
    }

    free(prev);
    free(curr);
}

int main()
{
    int n;
    printf("Enter matrix size (%d-%d): ", MINVAL, MAXVAL);
    scanf("%d", &n);

    if (n < MINVAL || n > MAXVAL) {
        printf("Invalid size! Must be between %d and %d.\n", MINVAL, MAXVAL);
        return 1;
    }

    srand(time(NULL));

    int *data = malloc(n * n * sizeof(int));     // the values
    int **mat = malloc(n * n * sizeof(int *));   // one pointer per cell

    generatematrix(mat, data, n);
    printf("Original Randomly Generated Matrix:\n");
    printmatrix(mat, n);

    rotatematrix(mat, n);
    printf("\nMatrix after 90 deg Clockwise Rotation:\n");
    printmatrix(mat, n);

    smoothmatrix(mat, n);
    printf("\nMatrix after Applying 3x3 Smoothing Filter:\n");
    printmatrix(mat, n);

    free(mat);
    free(data);
    return 0;
}



