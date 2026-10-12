#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_STUDENTS 100
#define NAME_LEN 50
#define LINE_LEN 128
#define SUBJECTS 3
#define MAX_MARKS 100

#define READ_OK 0
#define READ_EOF 1
#define READ_TOO_LONG 2

// Variable scope (1) - file scope
// Change 4 - float, same type as the average it is compared with
const float PASS_AVERAGE_LIMIT = 35.0f;

struct Student {
    int roll;
    char name[NAME_LEN];
    int marks[SUBJECTS];
};

struct Result {
    int total;
    float average;
    char grade;
};

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

void trimEnd(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

int readCount(int *count) {
    char line[LINE_LEN], extra;
    if (readLine(line, sizeof(line)) != READ_OK) return 0;
    if (sscanf(line, "%d %c", count, &extra) != 1) return 0;
    return *count >= 1 && *count <= MAX_STUDENTS;
}

// Change 1 - roll numbers must be unique
int rollExists(const struct Student *students, int count, int roll) {
    for (int i = 0; i < count; i++) {
        if (students[i].roll == roll) return 1;
    }
    return 0;
}

// Change 2 - %[^0-9] reads the name up to the first digit, so full names work
// Change 3 - name length is checked separately, after reading
int readStudent(struct Student *s, const struct Student *previous, int previousCount) {
    char line[LINE_LEN], name[LINE_LEN], extra;
    if (readLine(line, sizeof(line)) != READ_OK) return 0;

    int got = sscanf(line, "%d %127[^0-9] %d %d %d %c",
                     &s->roll, name, &s->marks[0], &s->marks[1], &s->marks[2], &extra);
    if (got != 5) return 0;

    if (s->roll <= 0 || rollExists(previous, previousCount, s->roll)) return 0;

    for (int i = 0; i < SUBJECTS; i++) {
        if (s->marks[i] < 0 || s->marks[i] > MAX_MARKS) return 0;
    }

    trimEnd(name);
    int len = (int)strlen(name);
    if (len == 0 || len >= NAME_LEN) return 0;
    for (int i = 0; i < len; i++) {
        if (!isalpha((unsigned char)name[i]) && name[i] != ' ') return 0;
    }
    strcpy(s->name, name);
    return 1;
}

int readStudents(struct Student *students, int count) {
    for (int i = 0; i < count; i++) {
        if (!readStudent(&students[i], students, i)) {
            printf("Error: invalid student details on line %d.\n", i + 1);
            printf("Use: Roll Name Marks1 Marks2 Marks3 (unique positive roll, name with letters and spaces, marks 0-%d).\n", MAX_MARKS);
            return 0;
        }
    }
    return 1;
}

int calculateTotal(const struct Student *s) {
    int total = 0;                           // Variable scope (2) - function scope
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

// Change 5 - processing and printing are separate functions
struct Result processStudent(const struct Student *s) {
    struct Result r;
    r.total = calculateTotal(s);
    r.average = calculateAverage(r.total);
    r.grade = assignGrade(r.average);
    return r;
}

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

void printRollNumbers(const struct Student *students, int index, int count) {
    if (index >= count) return;
    printf(" %d", students[index].roll);
    printRollNumbers(students, index + 1, count);
}

void printReport(const struct Student *students, int count) {
    for (int i = 0; i < count; i++) {
        struct Result result = processStudent(&students[i]);   // Variable scope (3) - block scope

        printResult(&students[i], &result);

        if (result.average < PASS_AVERAGE_LIMIT) continue;

        printf("Performance: ");
        printStars(starCount(result.grade));
    }

    printf("List of Roll Numbers (via recursion):");
    printRollNumbers(students, 0, count);
    printf("\n");
}

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
