#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MIN_SIZE 2
#define MAX_SIZE 10
#define MAX_PIXEL_VALUE 256

void clearInputLine(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
        ;
}

// reads a size in [MIN_SIZE, MAX_SIZE]; returns 0 on success, -1 on EOF
int readMatrixSize(int *n)
{
    while (1) {
        printf("Enter matrix size (%d-%d): ", MIN_SIZE, MAX_SIZE);

        int result = scanf("%d", n);
        if (result == EOF)
            return -1;

        int next = getchar();
        int isWholeLine = (next == '\n' || next == EOF);
        if (!isWholeLine)
            clearInputLine();

        if (result != 1 || !isWholeLine) {
            printf("Invalid input! Please enter a whole number.\n");
            continue;
        }

        if (*n < MIN_SIZE || *n > MAX_SIZE) {
            printf("Invalid size! Must be between %d and %d.\n", MIN_SIZE, MAX_SIZE);
            continue;
        }

        return 0;
    }
}

// point cell k of the matrix at its value in data
void linkMatrix(int **mat, int *data, int n)
{
    for (int k = 0; k < n * n; k++)
        *(mat + k) = data + k;
}

// fill every cell with a random value (0-255)
void generateMatrix(int **mat, int n)
{
    for (int k = 0; k < n * n; k++)
        **(mat + k) = rand() % MAX_PIXEL_VALUE;
}

void printMatrix(int **mat, int n)
{
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            printf("%4d", **(mat + i * n + j));
        printf("\n");
    }
}

// swap the pointers in two slots (addresses, not values)
void swapSlots(int **a, int **b)
{
    int *temp = *a;
    *a = *b;
    *b = temp;
}

// rotate 90 deg clockwise: transpose, then reverse each row
void rotateMatrix(int **mat, int n)
{
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            swapSlots(mat + i * n + j, mat + j * n + i);

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n / 2; j++)
            swapSlots(mat + i * n + j, mat + i * n + (n - 1 - j));
}

// value of cell (r, c) before smoothing: row i-1 from prev, row i from curr,
// row i+1 is still untouched in the matrix
int originalValue(int **mat, const int *prev, const int *curr, int n, int i, int r, int c)
{
    if (r == i - 1)
        return *(prev + c);
    if (r == i)
        return *(curr + c);
    return **(mat + r * n + c);
}

// average of the 3x3 neighbourhood around (i, j), clipped at the edges
int cellAverage(int **mat, const int *prev, const int *curr, int n, int i, int j)
{
    int sum = 0;
    int count = 0;

    for (int r = i - 1; r <= i + 1; r++) {
        if (r < 0 || r >= n)
            continue;

        for (int c = j - 1; c <= j + 1; c++) {
            if (c < 0 || c >= n)
                continue;

            sum += originalValue(mat, prev, curr, n, i, r, c);
            count++;
        }
    }

    return sum / count;
}

// 3x3 average using only two O(N) row buffers; returns 0 on success, -1 on allocation failure
int smoothMatrix(int **mat, int n)
{
    int *prev = malloc(n * sizeof(int));
    int *curr = malloc(n * sizeof(int));

    if (prev == NULL || curr == NULL) {
        free(prev);
        free(curr);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            *(curr + j) = **(mat + i * n + j);

        for (int j = 0; j < n; j++)
            **(mat + i * n + j) = cellAverage(mat, prev, curr, n, i, j);

        int *temp = prev;
        prev = curr;
        curr = temp;
    }

    free(prev);
    free(curr);
    return 0;
}

int main(void)
{
    int n;
    if (readMatrixSize(&n) != 0) {
        printf("\nNo input received.\n");
        return 1;
    }

    srand(time(NULL));

    int *data = malloc(n * n * sizeof(int));
    if (data == NULL) {
        fprintf(stderr, "Memory allocation failed!\n");
        return 1;
    }

    int **mat = malloc(n * n * sizeof(int *));
    if (mat == NULL) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(data);
        return 1;
    }

    linkMatrix(mat, data, n);
    generateMatrix(mat, n);
    printf("Original Randomly Generated Matrix:\n");
    printMatrix(mat, n);

    rotateMatrix(mat, n);
    printf("\nMatrix after 90 deg Clockwise Rotation:\n");
    printMatrix(mat, n);

    if (smoothMatrix(mat, n) != 0) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(mat);
        free(data);
        return 1;
    }
    printf("\nMatrix after Applying 3x3 Smoothing Filter:\n");
    printMatrix(mat, n);

    free(mat);
    free(data);
    return 0;
}
