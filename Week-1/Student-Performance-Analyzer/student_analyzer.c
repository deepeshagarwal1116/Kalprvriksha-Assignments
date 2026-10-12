#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_STUDENTS 100
#define NAME_LEN 50
#define LINE_LEN 128
#define SUBJECTS 3
#define MAX_MARKS 100

#define READ_OK 0
#define READ_EOF 1
#define READ_TOO_LONG 2

// Variable scope (1) - file scope: visible to every function below.
// It is a constant, so no function can change it by accident.
// Change 4 - declared as float, the same type as the average it is compared
// with, so there is no implicit float -> int conversion anywhere.
const float PASS_AVERAGE_LIMIT = 35.0f;   // average below this: no star pattern

struct Student {
    int roll;
    char name[NAME_LEN];
    int marks[SUBJECTS];
};

// Change 5 - the calculated values of one student live in their own struct,
// so calculating and printing are separate steps.
struct Result {
    int total;
    float average;
    char grade;
};

// read one whole line; returns READ_OK, READ_EOF, or READ_TOO_LONG
// (a too long line is fully consumed so it cannot leak into the next prompt)
int readLine(char *buf, int size) {
    if (fgets(buf, size, stdin) == NULL) return READ_EOF;

    char *nl = strchr(buf, '\n');
    if (nl) {
        *nl = '\0';
        return READ_OK;
    }
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
    return READ_TOO_LONG;
}

// remove spaces/tabs from the end of a string
void trimEnd(char *s) {
    int len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

// parse a whole token as an integer; returns 1 only if the entire token is a number
int parseInt(const char *token, int *value) {
    char *end;
    long n = strtol(token, &end, 10);
    if (end == token || *end != '\0') return 0;
    if (n < -1000000 || n > 1000000) return 0;
    *value = (int)n;
    return 1;
}

// cut the last space-separated word off the line and return it (NULL if none)
char *popLastToken(char *line) {
    trimEnd(line);
    int i = strlen(line) - 1;
    while (i >= 0 && !isspace((unsigned char)line[i])) i--;
    if (i < 0) return NULL;          // no space left: nothing before this word
    line[i] = '\0';
    return &line[i + 1];
}

// read the number of students; returns 1 if valid (1 <= N <= 100)
int readCount(int *count) {
    char line[LINE_LEN];
    if (readLine(line, sizeof(line)) != READ_OK) return 0;
    trimEnd(line);
    if (!parseInt(line, count)) return 0;
    return *count >= 1 && *count <= MAX_STUDENTS;
}

// Change 1 - roll numbers must be positive and unique, so we look through the
// students read so far before accepting a new roll number.
int rollExists(const struct Student *students, int count, int roll) {
    for (int i = 0; i < count; i++) {
        if (students[i].roll == roll) return 1;
    }
    return 0;
}

// Change 2 - names may contain spaces (e.g. "Arti Sharma"). The line is
// "Roll Name... M1 M2 M3": the last 3 words are marks, the first word is the
// roll number, and everything in between is the full name.
// Change 3 - the name length is checked on the extracted name itself (not by
// guessing from a %s limit), so a too long name is always rejected.
// returns 1 if valid, 0 otherwise (message is printed by the caller)
int readStudent(struct Student *s, const struct Student *previous, int previousCount) {
    char line[LINE_LEN];
    if (readLine(line, sizeof(line)) != READ_OK) return 0;   // EOF or too long line

    for (int i = SUBJECTS - 1; i >= 0; i--) {                // marks, from the end
        char *token = popLastToken(line);
        if (token == NULL || !parseInt(token, &s->marks[i])) return 0;
        if (s->marks[i] < 0 || s->marks[i] > MAX_MARKS) return 0;
    }

    char *rest = line;
    while (isspace((unsigned char)*rest)) rest++;
    char *end;
    long roll = strtol(rest, &end, 10);                       // roll number, first word
    if (end == rest || !isspace((unsigned char)*end)) return 0;
    if (roll <= 0 || roll > 1000000) return 0;                // must be positive
    s->roll = (int)roll;
    if (rollExists(previous, previousCount, s->roll)) return 0;   // must be unique

    char *name = end;
    while (isspace((unsigned char)*name)) name++;
    trimEnd(name);
    int len = strlen(name);
    if (len == 0 || len >= NAME_LEN) return 0;                // empty or too long
    for (int i = 0; i < len; i++) {                           // letters and spaces only
        if (!isalpha((unsigned char)name[i]) && name[i] != ' ') return 0;
    }
    strcpy(s->name, name);
    return 1;
}

// read all students; returns 1 if every line was valid
int readStudents(struct Student *students, int count) {
    for (int i = 0; i < count; i++) {
        if (!readStudent(&students[i], students, i)) {
            printf("Error: invalid student details on line %d.\n", i + 1);
            printf("Use: Roll Name Marks1 Marks2 Marks3 (roll must be positive and unique, marks 0-%d, name letters and spaces only).\n", MAX_MARKS);
            return 0;
        }
    }
    return 1;
}

int calculateTotal(const struct Student *s) {
    int total = 0;                           // Variable scope (2) - local to this function
    for (int i = 0; i < SUBJECTS; i++) {
        total += s->marks[i];
    }
    return total;
}

float calculateAverage(int total) {
    return (float)total / SUBJECTS;
}

char assignGrade(float average) {
    if (average >= 85.0f) return 'A';
    else if (average >= 70.0f) return 'B';
    else if (average >= 50.0f) return 'C';
    else if (average >= PASS_AVERAGE_LIMIT) return 'D';
    return 'F';
}

int starCount(char grade) {
    switch (grade) {
        case 'A': return 5;
        case 'B': return 4;
        case 'C': return 3;
        case 'D': return 2;
        default:  return 0;
    }
}

// Change 5 - student processing: only calculates, prints nothing
struct Result processStudent(const struct Student *s) {
    struct Result r;
    r.total = calculateTotal(s);
    r.average = calculateAverage(r.total);
    r.grade = assignGrade(r.average);
    return r;
}

// Change 5 - result printing: only prints, calculates nothing
void printResult(const struct Student *s, const struct Result *r) {
    printf("Roll: %d\n", s->roll);
    printf("Name: %s\n", s->name);
    printf("Total: %d\n", r->total);
    printf("Average: %.2f\n", r->average);
    printf("Grade: %c\n", r->grade);
}

void printStars(int count) {
    for (int i = 0; i < count; i++) printf("*");
    printf("\n");
}

// recursion: print roll numbers from index to the last student
void printRollNumbers(const struct Student *students, int index, int count) {
    if (index >= count) return;              // base case
    printf(" %d", students[index].roll);
    printRollNumbers(students, index + 1, count);
}

// Change 5 - report: loops over the students and prints each result
void printReport(const struct Student *students, int count) {
    for (int i = 0; i < count; i++) {
        // Variable scope (3) - block scope: this exists only inside this loop body,
        // so each student gets a fresh value on every iteration.
        struct Result result = processStudent(&students[i]);

        printResult(&students[i], &result);

        if (result.average < PASS_AVERAGE_LIMIT) continue;   // below 35: skip the star pattern

        printf("Performance: ");
        printStars(starCount(result.grade));
    }

    printf("List of Roll Numbers (via recursion):");
    printRollNumbers(students, 0, count);
    printf("\n");
}

// Change 5 - main() only reads the input and calls the other functions
int main() {
    int count;
    if (!readCount(&count)) {
        printf("Error: number of students must be between 1 and %d.\n", MAX_STUDENTS);
        return 1;
    }

    struct Student students[MAX_STUDENTS];
    if (!readStudents(students, count)) return 1;

    printReport(students, count);
    return 0;
}
