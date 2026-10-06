#include <stdio.h>
#include <string.h>

// Change 1 - named constants for file names and sizes instead of
// repeating "users.txt" and 50 everywhere.
#define DATA_FILE "users.txt"
#define TEMP_FILE "temp.txt"
#define NAME_LEN 50
#define LINE_LEN 128

struct User {
    int id;
    char name[NAME_LEN];
    int age;
};

// Change 2 - line-based, bounded input helpers (readLine, readInt, readNameAge)
// replace scanf("%s"). A name longer than the buffer can no longer overflow
// u.name; it is rejected with an error instead.

// read one whole line from the user; returns 1 on success, 0 on EOF
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

// read a single integer on its own line; returns 1 if valid
int readInt(int *value) {
    char line[LINE_LEN], extra;
    if (!readLine(line, sizeof(line))) return 0;
    return sscanf(line, "%d %c", value, &extra) == 1;
}

// read "Name Age" from one line with a bounded name; returns 1 if valid
int readNameAge(char *name, int *age) {
    char line[LINE_LEN], extra;
    if (!readLine(line, sizeof(line))) return 0;
    if (sscanf(line, "%49s %d %c", name, age, &extra) != 2) return 0;
    if (strlen(name) >= NAME_LEN - 1) return 0;   // name was too long and got cut
    return *age > 0;
}

// Change 3 - check whether an ID already exists, so IDs stay unique on add
// and update/delete can report "User not found".
int idExists(int id) {
    struct User u;
    FILE *fp = fopen(DATA_FILE, "r");
    if (fp == NULL) return 0;

    int found = 0;
    while (fscanf(fp, "%d %49s %d", &u.id, u.name, &u.age) == 3) {
        if (u.id == id) { found = 1; break; }
    }
    fclose(fp);
    return found;
}

// Change 4 - return values of remove() and rename() are checked; success is
// only reported if both worked, otherwise an error is printed.
int replaceDataFile() {
    if (remove(DATA_FILE) != 0) {
        printf("Error: could not remove %s\n", DATA_FILE);
        remove(TEMP_FILE);
        return 0;
    }
    if (rename(TEMP_FILE, DATA_FILE) != 0) {
        printf("Error: could not rename %s to %s (data kept in %s)\n", TEMP_FILE, DATA_FILE, TEMP_FILE);
        return 0;
    }
    return 1;
}

void addUser() {
    struct User u;
    printf("Enter ID: ");
    if (!readInt(&u.id) || u.id <= 0) { printf("Invalid ID\n"); return; }

    // Change 3 (used) - reject duplicate IDs before writing to the file
    if (idExists(u.id)) { printf("Error: user with ID %d already exists\n", u.id); return; }

    printf("Enter Name (no spaces, max %d chars) and Age: ", NAME_LEN - 2);
    if (!readNameAge(u.name, &u.age)) { printf("Invalid Name or Age\n"); return; }

    FILE *fp = fopen(DATA_FILE, "a");
    if (fp == NULL) { printf("Error: could not open %s\n", DATA_FILE); return; }
    fprintf(fp, "%d %s %d\n", u.id, u.name, u.age);
    fclose(fp);
    printf("User added\n");
}

void showUsers() {
    struct User u;
    FILE *fp = fopen(DATA_FILE, "r");
    if (fp == NULL) { printf("Error: could not open %s\n", DATA_FILE); return; }

    while (fscanf(fp, "%d %49s %d", &u.id, u.name, &u.age) == 3) {
        printf("%d %s %d\n", u.id, u.name, u.age);
    }
    fclose(fp);
}

void updateUser() {
    struct User u;
    int id;
    printf("Enter ID to update: ");
    if (!readInt(&id)) { printf("Invalid ID\n"); return; }

    if (!idExists(id)) { printf("User not found\n"); return; }

    char newName[NAME_LEN];
    int newAge;
    printf("Enter new Name and Age: ");
    if (!readNameAge(newName, &newAge)) { printf("Invalid Name or Age\n"); return; }

    FILE *fp = fopen(DATA_FILE, "r");
    if (fp == NULL) { printf("Error: could not open %s\n", DATA_FILE); return; }
    FILE *temp = fopen(TEMP_FILE, "w");
    if (temp == NULL) { printf("Error: could not open %s\n", TEMP_FILE); fclose(fp); return; }

    while (fscanf(fp, "%d %49s %d", &u.id, u.name, &u.age) == 3) {
        if (u.id == id) {
            strcpy(u.name, newName);
            u.age = newAge;
        }
        fprintf(temp, "%d %s %d\n", u.id, u.name, u.age);
    }
    fclose(fp);
    fclose(temp);

    if (replaceDataFile()) printf("User updated\n");
}

void deleteUser() {
    struct User u;
    int id;
    printf("Enter ID to delete: ");
    if (!readInt(&id)) { printf("Invalid ID\n"); return; }

    if (!idExists(id)) { printf("User not found\n"); return; }

    FILE *fp = fopen(DATA_FILE, "r");
    if (fp == NULL) { printf("Error: could not open %s\n", DATA_FILE); return; }
    FILE *temp = fopen(TEMP_FILE, "w");
    if (temp == NULL) { printf("Error: could not open %s\n", TEMP_FILE); fclose(fp); return; }

    while (fscanf(fp, "%d %49s %d", &u.id, u.name, &u.age) == 3) {
        if (u.id != id) {
            fprintf(temp, "%d %s %d\n", u.id, u.name, u.age);
        }
    }
    fclose(fp);
    fclose(temp);

    if (replaceDataFile()) printf("User deleted\n");
}

int main() {
    int choice;

    // Change 5 - every fopen() result is checked before use (here and in all
    // other functions), so we never call fclose(NULL) or read from a NULL file.
    FILE *fp = fopen(DATA_FILE, "a");   // creates file if not there
    if (fp == NULL) {
        printf("Error: could not create or open %s\n", DATA_FILE);
        return 1;
    }
    fclose(fp);

    while (1) {
        printf("\n1.Add 2.Show 3.Update 4.Delete 5.Exit\nChoice: ");
        // Change 6 - menu choice is validated; only 5 exits, anything else
        // invalid prints a message instead of quitting.
        if (!readInt(&choice)) {
            if (feof(stdin)) break;
            printf("Invalid choice\n");
            continue;
        }

        if (choice == 1) addUser();
        else if (choice == 2) showUsers();
        else if (choice == 3) updateUser();
        else if (choice == 4) deleteUser();
        else if (choice == 5) break;
        else printf("Invalid choice\n");
    }
    return 0;
}
