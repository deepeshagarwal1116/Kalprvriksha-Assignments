#include <stdio.h>

struct User {
    int id;
    char name[50];
    int age;
};

void addUser() {
    struct User u;
    printf("Enter ID, Name, Age: ");
    scanf("%d %s %d", &u.id, u.name, &u.age);

    FILE *fp = fopen("users.txt", "a");
    fprintf(fp, "%d %s %d\n", u.id, u.name, u.age);
    fclose(fp);
    printf("User added\n");
}

void showUsers() {
    struct User u;
    FILE *fp = fopen("users.txt", "r");

    while (fscanf(fp, "%d %s %d", &u.id, u.name, &u.age) == 3) {
        printf("%d %s %d\n", u.id, u.name, u.age);
    }
    fclose(fp);
}

void updateUser() {
    struct User u;
    int id;
    printf("Enter ID to update: ");
    scanf("%d", &id);

    FILE *fp = fopen("users.txt", "r");
    FILE *temp = fopen("temp.txt", "w");

    while (fscanf(fp, "%d %s %d", &u.id, u.name, &u.age) == 3) {
        if (u.id == id) {
            printf("Enter new Name, Age: ");
            scanf("%s %d", u.name, &u.age);
        }
        fprintf(temp, "%d %s %d\n", u.id, u.name, u.age);
    }
    fclose(fp);
    fclose(temp);

    remove("users.txt");
    rename("temp.txt", "users.txt");
}

void deleteUser() {
    struct User u;
    int id;
    printf("Enter ID to delete: ");
    scanf("%d", &id);

    FILE *fp = fopen("users.txt", "r");
    FILE *temp = fopen("temp.txt", "w");

    while (fscanf(fp, "%d %s %d", &u.id, u.name, &u.age) == 3) {
        if (u.id != id) {
            fprintf(temp, "%d %s %d\n", u.id, u.name, u.age);
        }
    }
    fclose(fp);
    fclose(temp);

    remove("users.txt");
    rename("temp.txt", "users.txt");
}

int main() {
    int choice;

    FILE *fp = fopen("users.txt", "a");   // creates file if not there
    fclose(fp);

    while (1) {
        printf("\n1.Add 2.Show 3.Update 4.Delete 5.Exit\nChoice: ");
        scanf("%d", &choice);

        if (choice == 1) addUser();
        else if (choice == 2) showUsers();
        else if (choice == 3) updateUser();
        else if (choice == 4) deleteUser();
        else break;
    }
    return 0;
}
