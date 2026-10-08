#include <stdio.h>
#include <string.h>

#define MAX_STUDENTS 100
#define NAME_LEN 50
#define LINE_LEN 128
#define SUBJECTS 3
#define MAX_MARKS 100
#define FAIL_BELOW 35.0f   // failing condn

// It is a constant, so no function can change it by accident.
const int PASS_MARKS_LIMIT = FAIL_BELOW;

struct Student {
    int roll;
    char name[NAME_LEN];
    int marks[SUBJECTS];
};

// read one whole line; returns 1 on success, 0 on EOF
int readLine(char *buf, int size) {
    if (fgets(buf, size, stdin) == NULL) return 0;

    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    else {                                   // line too long: drop the rest of it
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF);
    }
    return 1;
}

// read the number of students; returns 1 if valid (1 <= N <= 100)
int readCount(int *count) {
    char line[LINE_LEN], extra;
    if (!readLine(line, sizeof(line))) return 0;
    if (sscanf(line, "%d %c", count, &extra) != 1) return 0;
    return *count >= 1 && *count <= MAX_STUDENTS;
}

// read "Roll Name M1 M2 M3" from one line; returns 1 if valid
int readStudent(struct Student *s) {
    char line[LINE_LEN], extra;
    if (!readLine(line, sizeof(line))) return 0;

    int got = sscanf(line, "%d %49s %d %d %d %c", &s->roll, s->name,
                    &s->marks[0], &s->marks[1], &s->marks[2], &extra);
    if (got != 5) return 0;
    if (strlen(s->name) >= NAME_LEN - 1) return 0;   // name was too long and got cut

    for (int i = 0; i < SUBJECTS; i++) {
        if (s->marks[i] < 0 || s->marks[i] > MAX_MARKS) return 0;
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
    if (average >= 85) return 'A';
    else if (average >= 70) return 'B';
    else if (average >= 50) return 'C';
    else if (average >= 35) return 'D';
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

int main() {
    int count;
    if (!readCount(&count)) {
        printf("Error: number of students must be between 1 and %d.\n", MAX_STUDENTS);
        return 1;
    }

    struct Student students[MAX_STUDENTS];
    for (int i = 0; i < count; i++) {
        if (!readStudent(&students[i])) {
            printf("Error: invalid student details. Use: Roll Name Marks1 Marks2 Marks3 (marks 0-%d).\n", MAX_MARKS);
            return 1;
        }
    }

    for (int i = 0; i < count; i++) {
        // so each student gets fresh values on every iteration.
        int total = calculateTotal(&students[i]);
        float average = calculateAverage(total);
        char grade = assignGrade(average);

        printf("Roll: %d\n", students[i].roll);
        printf("Name: %s\n", students[i].name);
        printf("Total: %d\n", total);
        printf("Average: %.2f\n", average);
        printf("Grade: %c\n", grade);

        if (average < PASS_MARKS_LIMIT) continue;   // below 35: skip the star pattern

        printf("Performance: ");
        printStars(starCount(grade));
    }

    printf("List of Roll Numbers (via recursion):");
    printRollNumbers(students, 0, count);
    printf("\n");
    return 0;
}
