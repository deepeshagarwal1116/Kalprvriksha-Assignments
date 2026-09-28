#include <stdio.h>
#include <ctype.h>

int numSt[100], numTop = -1;   // stack for numbers
char opSt[100];  int opTop = -1; // stack for operators
int divByZero = 0;

int precedence(char op) {
    if (op == '*' || op == '/') return 2;
    return 1; // + and -
}

// pop 2 numbers and 1 operator, calculate, push result back
void applyTop() {
    int b = numSt[numTop--];
    int a = numSt[numTop--];
    char op = opSt[opTop--];
    int result = 0;

    if (op == '+') result = a + b;
    else if (op == '-') result = a - b;
    else if (op == '*') result = a * b;
    else if (op == '/') {
        if (b == 0) divByZero = 1;
        else result = a / b;
    }
    numSt[++numTop] = result;
}

int main() {
    char exp[200];
    fgets(exp, sizeof(exp), stdin);

    int numflag = 1; // expression must start with a number

    for (int i = 0; exp[i] != '\0' && exp[i] != '\n'; i++) {
        char c = exp[i];

        if (c == ' ' || c == '"') continue;   // skip spaces (and quotes)

        if (isdigit(c)) {
            if (!numflag) { printf("Error: Invalid expression.\n"); return 0; }
            int num = 0;
            while (isdigit(exp[i])) {          // read full number, e.g. "123"
                num = num * 10 + (exp[i] - '0');
                i++;
            }
            i--;                               // loop will do i++ again
            numSt[++numTop] = num;
            numflag = 0;
        }
        else if (c == '+' || c == '-' || c == '*' || c == '/') {
            if (numflag) { printf("Error: Invalid expression.\n"); return 0; }
            // solve earlier operators that have same or higher priority
            while (opTop >= 0 && precedence(opSt[opTop]) >= precedence(c))
                applyTop();
            opSt[++opTop] = c;
            numflag = 1;
        }
        else {                                  // any other character
            printf("Error: Invalid expression.\n");
            return 0;
        }
    }

    if (numflag) { printf("Error: Invalid expression.\n"); return 0; } // empty or ends with operator

    while (opTop >= 0) applyTop();              // solve remaining operators

    if (divByZero) printf("Error: Division by zero.\n");
    else printf("%d\n", numSt[numTop]);
    return 0;
}
