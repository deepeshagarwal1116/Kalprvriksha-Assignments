#include <stdio.h>
#include <ctype.h>

// Change 1 - named constants instead of hardcoding 100 / 200,
// so the stack and input sizes can be changed from one place.
#define MAX_STACK_SIZE 100
#define MAX_EXPR_LEN 200

// Change 2 - status codes so every function can report an error
// back to the caller instead of setting a global flag.
#define OK 0
#define ERR_INVALID 1
#define ERR_DIV_ZERO 2
#define ERR_OVERFLOW 3

// Change 3 - all evaluator state lives in this struct instead of global
// variables (numSt, opSt, numTop, opTop, divByZero). It is passed to the
// functions that need it, so there are no hidden dependencies.
struct Calculator {
    int numSt[MAX_STACK_SIZE];   // stack for numbers
    int numTop;
    char opSt[MAX_STACK_SIZE];   // stack for operators
    int opTop;
};

int precedence(char op) {
    if (op == '*' || op == '/') return 2;
    return 1; // + and -
}

// Change 4 - bounds check before every push, so a long expression
// returns an error instead of writing past the end of the array.
int pushNum(struct Calculator *calc, int num) {
    if (calc->numTop >= MAX_STACK_SIZE - 1) return ERR_OVERFLOW;
    calc->numSt[++calc->numTop] = num;
    return OK;
}

int pushOp(struct Calculator *calc, char op) {
    if (calc->opTop >= MAX_STACK_SIZE - 1) return ERR_OVERFLOW;
    calc->opSt[++calc->opTop] = op;
    return OK;
}

// pop 2 numbers and 1 operator, calculate, push result back
// Change 5 - returns an error status; on division by zero it stops right
// away and does NOT push a wrong result onto the stack.
int applyTop(struct Calculator *calc) {
    if (calc->numTop < 1 || calc->opTop < 0) return ERR_INVALID;

    int b = calc->numSt[calc->numTop--];
    int a = calc->numSt[calc->numTop--];
    char op = calc->opSt[calc->opTop--];
    int result = 0;

    if (op == '+') result = a + b;
    else if (op == '-') result = a - b;
    else if (op == '*') result = a * b;
    else if (op == '/') {
        if (b == 0) return ERR_DIV_ZERO;   // stop right away, don't push a bad result
        result = a / b;
    }
    return pushNum(calc, result);
}

// Change 6 - expression logic moved out of main() into evaluate(), which
// creates its own Calculator and returns a status; the result goes in *answer.
int evaluate(const char *exp, int *answer) {
    struct Calculator calc;
    calc.numTop = -1;
    calc.opTop = -1;

    int numflag = 1; // expression must start with a number
    int status;

    for (int i = 0; exp[i] != '\0' && exp[i] != '\n'; i++) {
        char c = exp[i];

        // Change 7 - only whitespace is skipped. Quotes are no longer ignored,
        // so 3 + "5" now gives "Error: Invalid expression."
        if (isspace((unsigned char)c)) continue;

        if (isdigit((unsigned char)c)) {
            if (!numflag) return ERR_INVALID;
            int num = 0;
            while (isdigit((unsigned char)exp[i])) {   // read full number, e.g. "123"
                num = num * 10 + (exp[i] - '0');
                i++;
            }
            i--;                                       // loop will do i++ again
            status = pushNum(&calc, num);
            if (status != OK) return status;
            numflag = 0;
        }
        else if (c == '+' || c == '-' || c == '*' || c == '/') {
            if (numflag) return ERR_INVALID;
            // solve earlier operators that have same or higher priority
            while (calc.opTop >= 0 && precedence(calc.opSt[calc.opTop]) >= precedence(c)) {
                status = applyTop(&calc);
                if (status != OK) return status;
            }
            status = pushOp(&calc, c);
            if (status != OK) return status;
            numflag = 1;
        }
        else {                                          // any other character (e.g. quotes)
            return ERR_INVALID;
        }
    }

    if (numflag) return ERR_INVALID;                    // empty or ends with operator

    while (calc.opTop >= 0) {                           // solve remaining operators
        status = applyTop(&calc);
        if (status != OK) return status;
    }

    *answer = calc.numSt[calc.numTop];
    return OK;
}

int main() {
    char exp[MAX_EXPR_LEN];
    if (fgets(exp, sizeof(exp), stdin) == NULL) {
        printf("Error: Invalid expression.\n");
        return 0;
    }

    // Change 8 - main() only reads input and prints the result or error message
    int answer;
    int status = evaluate(exp, &answer);

    if (status == OK) printf("%d\n", answer);
    else if (status == ERR_DIV_ZERO) printf("Error: Division by zero.\n");
    else if (status == ERR_OVERFLOW) printf("Error: Expression too long.\n");
    else printf("Error: Invalid expression.\n");
    return 0;
}
