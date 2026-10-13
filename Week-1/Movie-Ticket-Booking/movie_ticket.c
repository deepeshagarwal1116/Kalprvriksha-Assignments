#include <stdio.h>

#define MAX_NAME_LEN 50
#define MIN_TICKETS 1
#define MAX_TICKETS 20
#define DISCOUNT_MIN_TICKETS 5
#define DISCOUNT_RATE 0.10

const double STANDARD_PRICE = 150;
const double PREMIUM_PRICE = 250;
const double RECLINER_PRICE = 400;

struct Customer {
    char name[MAX_NAME_LEN];
    int noOfTickets;
    int categoryChoice;
};

// reads and validates the input; returns 1 if valid, 0 otherwise
int takeInputs(struct Customer *c) {
    printf("Enter the customer name: ");
    if (scanf("%49s", c->name) != 1) {
        printf("Error: invalid name.\n");
        return 0;
    }

    printf("Enter the number of tickets: ");
    if (scanf("%d", &c->noOfTickets) != 1 ||
        c->noOfTickets < MIN_TICKETS || c->noOfTickets > MAX_TICKETS) {
        printf("Error: number of tickets must be between %d and %d.\n", MIN_TICKETS, MAX_TICKETS);
        return 0;
    }

    printf("Enter category (1 = Standard, 2 = Premium, 3 = Recliner): ");
    if (scanf("%d", &c->categoryChoice) != 1 ||
        c->categoryChoice < 1 || c->categoryChoice > 3) {
        printf("Error: category must be 1, 2 or 3.\n");
        return 0;
    }
    return 1;
}

double getPrice(int category) {
    switch (category) {
        case 1: return STANDARD_PRICE;
        case 2: return PREMIUM_PRICE;
        case 3: return RECLINER_PRICE;
        default: return 0;
    }
}

const char *getCategoryName(int category) {
    switch (category) {
        case 1: return "Standard";
        case 2: return "Premium";
        case 3: return "Recliner";
        default: return "Unknown";
    }
}

void showOutput(const struct Customer *c) {
    double subtotal = getPrice(c->categoryChoice) * c->noOfTickets;
    double discount;

    if (c->noOfTickets >= DISCOUNT_MIN_TICKETS) {
        discount = subtotal * DISCOUNT_RATE;
    } else {
        discount = 0;
    }
    double finalBill = subtotal - discount;

    printf("Customer: %s\n", c->name);
    printf("Ticket Category: %s\n", getCategoryName(c->categoryChoice));
    printf("Tickets: %d\n", c->noOfTickets);
    printf("Subtotal: %.2f\n", subtotal);
    printf("Discount: %.2f\n", discount);
    printf("Final Bill: %.2f\n", finalBill);
}

int main() {
    struct Customer c1;

    if (!takeInputs(&c1)) return 1;
    showOutput(&c1);
    return 0;
}
