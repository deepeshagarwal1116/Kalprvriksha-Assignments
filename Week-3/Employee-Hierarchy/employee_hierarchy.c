#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_REPORTS 50
#define NAME_LEN 50
#define LINE_LEN 100
#define INDENT 4

enum Choice { ADD = 1, REMOVE, SHOW_EMPLOYEE, SHOW_ALL, EXIT };

enum Status {
    OK,
    ERR_DUPLICATE_ID,
    ERR_MANAGER_NOT_FOUND,
    ERR_SELF_MANAGER,
    ERR_FULL,
    ERR_MEMORY,
    ERR_NOT_FOUND,
    ERR_REMOVE_CEO
};

struct Employee {
    int id;
    char name[NAME_LEN];
    char designation[NAME_LEN];
    struct Employee *manager;
    struct Employee *reports[MAX_REPORTS];
    int reportCount;
};

struct Organization {
    struct Employee *ceo;
};

int readLine(char *line, int size) {
    if (fgets(line, size, stdin) == NULL) return 0;

    char *newline = strchr(line, '\n');
    if (newline != NULL) {
        *newline = '\0';
        return 1;
    }
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
    return -1;
}

int readInt(const char *prompt, int *value) {
    char line[LINE_LEN], extra;
    printf("%s", prompt);
    if (readLine(line, sizeof(line)) != 1) return 0;
    return sscanf(line, "%d %c", value, &extra) == 1 && *value > 0;
}

int readText(const char *prompt, char *text, int size) {
    printf("%s", prompt);
    if (readLine(text, size) != 1) return 0;

    for (int i = 0; text[i] != '\0'; i++) {
        if (!isspace((unsigned char)text[i])) return 1;
    }
    return 0;
}

struct Employee *findEmployee(struct Employee *employee, int id) {
    if (employee == NULL) return NULL;
    if (employee->id == id) return employee;

    for (int i = 0; i < employee->reportCount; i++) {
        struct Employee *found = findEmployee(employee->reports[i], id);
        if (found != NULL) return found;
    }
    return NULL;
}

int countUnder(const struct Employee *employee) {
    int total = 0;
    for (int i = 0; i < employee->reportCount; i++) {
        total += 1 + countUnder(employee->reports[i]);
    }
    return total;
}

void printTree(const struct Employee *employee, int depth) {
    printf("%*s%d - %s - %s\n", depth * INDENT, "", employee->id, employee->name, employee->designation);
    for (int i = 0; i < employee->reportCount; i++) {
        printTree(employee->reports[i], depth + 1);
    }
}

void freeTree(struct Employee *employee) {
    if (employee == NULL) return;
    for (int i = 0; i < employee->reportCount; i++) {
        freeTree(employee->reports[i]);
    }
    free(employee);
}

struct Employee *createEmployee(int id, const char *name, const char *designation) {
    struct Employee *employee = (struct Employee *)malloc(sizeof(struct Employee));
    if (employee == NULL) return NULL;

    employee->id = id;
    strcpy(employee->name, name);
    strcpy(employee->designation, designation);
    employee->manager = NULL;
    employee->reportCount = 0;
    return employee;
}

enum Status addEmployee(struct Organization *org, int id, const char *name,
                        const char *designation, int managerId) {
    if (findEmployee(org->ceo, id) != NULL) return ERR_DUPLICATE_ID;

    struct Employee *manager = NULL;
    if (org->ceo != NULL) {
        if (managerId == id) return ERR_SELF_MANAGER;
        manager = findEmployee(org->ceo, managerId);
        if (manager == NULL) return ERR_MANAGER_NOT_FOUND;
        if (manager->reportCount >= MAX_REPORTS) return ERR_FULL;
    }

    struct Employee *employee = createEmployee(id, name, designation);
    if (employee == NULL) return ERR_MEMORY;

    if (manager == NULL) {
        org->ceo = employee;
    } else {
        employee->manager = manager;
        manager->reports[manager->reportCount++] = employee;
    }
    return OK;
}

struct Employee *chooseTarget(struct Employee *manager, const struct Employee *removed) {
    struct Employee *target = manager;
    int fewest = -1;

    for (int i = 0; i < manager->reportCount; i++) {
        struct Employee *sibling = manager->reports[i];
        if (sibling == removed) continue;
        if (fewest == -1 || sibling->reportCount < fewest) {
            fewest = sibling->reportCount;
            target = sibling;
        }
    }
    return target;
}

void detachFromManager(struct Employee *employee) {
    struct Employee *manager = employee->manager;
    int position = 0;

    while (manager->reports[position] != employee) position++;
    for (int i = position; i < manager->reportCount - 1; i++) {
        manager->reports[i] = manager->reports[i + 1];
    }
    manager->reportCount--;
}

enum Status removeEmployee(struct Organization *org, int id) {
    struct Employee *employee = findEmployee(org->ceo, id);
    if (employee == NULL) return ERR_NOT_FOUND;
    if (employee->manager == NULL) return ERR_REMOVE_CEO;

    struct Employee *manager = employee->manager;
    struct Employee *target = chooseTarget(manager, employee);

    int targetCount = target->reportCount;
    if (target == manager) targetCount--;
    if (targetCount + employee->reportCount > MAX_REPORTS) return ERR_FULL;

    detachFromManager(employee);
    for (int i = 0; i < employee->reportCount; i++) {
        struct Employee *report = employee->reports[i];
        report->manager = target;
        target->reports[target->reportCount++] = report;
    }
    free(employee);
    return OK;
}

void showStatus(enum Status status) {
    switch (status) {
        case ERR_DUPLICATE_ID:
            printf("Error: an employee with this ID already exists.\n");
            break;
        case ERR_MANAGER_NOT_FOUND:
            printf("Error: the reporting manager does not exist.\n");
            break;
        case ERR_SELF_MANAGER:
            printf("Error: an employee cannot report to themselves.\n");
            break;
        case ERR_FULL:
            printf("Error: a manager can have at most %d direct reports.\n", MAX_REPORTS);
            break;
        case ERR_MEMORY:
            printf("Error: out of memory.\n");
            break;
        case ERR_NOT_FOUND:
            printf("Error: employee not found.\n");
            break;
        case ERR_REMOVE_CEO:
            printf("Error: the CEO cannot be removed.\n");
            break;
        default:
            break;
    }
}

void handleAdd(struct Organization *org) {
    int id, managerId = 0;
    char name[NAME_LEN], designation[NAME_LEN];

    if (!readInt("Enter Employee ID: ", &id)) {
        printf("Error: ID must be a positive number.\n");
        return;
    }
    if (!readText("Enter Name: ", name, sizeof(name)) ||
        !readText("Enter Designation: ", designation, sizeof(designation))) {
        printf("Error: name and designation cannot be empty or too long.\n");
        return;
    }
    if (org->ceo == NULL) {
        printf("First employee becomes the CEO and has no reporting manager.\n");
    } else if (!readInt("Enter Reporting Manager ID: ", &managerId)) {
        printf("Error: every employee except the CEO needs a reporting manager ID.\n");
        return;
    }

    enum Status status = addEmployee(org, id, name, designation, managerId);
    if (status == OK) printf("Employee added.\n");
    else showStatus(status);
}

void handleRemove(struct Organization *org) {
    int id;
    if (!readInt("Enter Employee ID to remove: ", &id)) {
        printf("Error: ID must be a positive number.\n");
        return;
    }

    enum Status status = removeEmployee(org, id);
    if (status == OK) printf("Employee removed.\n");
    else showStatus(status);
}

void handleShowEmployee(const struct Organization *org) {
    int id;
    if (!readInt("Enter Employee ID: ", &id)) {
        printf("Error: ID must be a positive number.\n");
        return;
    }

    struct Employee *employee = findEmployee(org->ceo, id);
    if (employee == NULL) {
        showStatus(ERR_NOT_FOUND);
        return;
    }

    printf("\n");
    printTree(employee, 0);
    printf("\nTotal Employees Under %d: %d\n", id, countUnder(employee));
}

void handleShowAll(const struct Organization *org) {
    if (org->ceo == NULL) {
        printf("The organization is empty.\n");
        return;
    }

    printf("\n===== ORGANIZATION HIERARCHY =====\n\n");
    printTree(org->ceo, 0);
    printf("\nTotal Employees: %d\n", 1 + countUnder(org->ceo));
}

int main() {
    struct Organization org;
    org.ceo = NULL;

    while (1) {
        printf("\n===== EMPLOYEE HIERARCHY MANAGEMENT =====\n\n");
        printf("1. Add Employee\n2. Remove Employee\n3. Display Employee Hierarchy\n");
        printf("4. Display Complete Hierarchy\n5. Exit\n\n");

        int choice;
        if (!readInt("Enter your choice: ", &choice)) {
            if (feof(stdin)) break;
            printf("Invalid choice. Enter a number from 1 to 5.\n");
            continue;
        }

        if (choice == ADD) handleAdd(&org);
        else if (choice == REMOVE) handleRemove(&org);
        else if (choice == SHOW_EMPLOYEE) handleShowEmployee(&org);
        else if (choice == SHOW_ALL) handleShowAll(&org);
        else if (choice == EXIT) break;
        else printf("Invalid choice. Enter a number from 1 to 5.\n");
    }

    freeTree(org.ceo);
    return 0;
}
