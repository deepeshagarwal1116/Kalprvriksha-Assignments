#include <stdio.h>

enum {
    MAX_NAME_LEN = 50,
    MIN_TICKETS = 1,
    MAX_TICKETS = 20,
    DISCOUNT_MIN_TICKETS = 5
};

const double DISCOUNT_RATE = 0.10;
const double STANDARD_PRICE = 150;
const double PREMIUM_PRICE = 250;
const double RECLINER_PRICE = 400;

enum Category { STANDARD = 1, PREMIUM = 2, RECLINER = 3 };

enum InputStatus { INPUT_OK, ERR_NAME, ERR_TICKETS, ERR_CATEGORY };

struct Customer {
    char name[MAX_NAME_LEN];
    int noOfTickets;
    int categoryChoice;
};

struct CategoryInfo {
    const char *name;
    double price;
};

int getCategoryInfo(int category, struct CategoryInfo *info) {
    switch (category) {
        case STANDARD:
            info->name = "Standard";
            info->price = STANDARD_PRICE;
            return 1;
        case PREMIUM:
            info->name = "Premium";
            info->price = PREMIUM_PRICE;
            return 1;
        case RECLINER:
            info->name = "Recliner";
            info->price = RECLINER_PRICE;
            return 1;
        default:
            return 0;
    }
}

enum InputStatus takeInputs(struct Customer *customer) {
    struct CategoryInfo info;

    if (scanf("%49s", customer->name) != 1) return ERR_NAME;

    if (scanf("%d", &customer->noOfTickets) != 1 ||
        customer->noOfTickets < MIN_TICKETS || customer->noOfTickets > MAX_TICKETS) {
        return ERR_TICKETS;
    }

    if (scanf("%d", &customer->categoryChoice) != 1 ||
        !getCategoryInfo(customer->categoryChoice, &info)) {
        return ERR_CATEGORY;
    }
    return INPUT_OK;
}

void showError(enum InputStatus status) {
    if (status == ERR_NAME) {
        printf("Error: invalid name.\n");
    } else if (status == ERR_TICKETS) {
        printf("Error: number of tickets must be between %d and %d.\n", MIN_TICKETS, MAX_TICKETS);
    } else if (status == ERR_CATEGORY) {
        printf("Error: category must be 1, 2 or 3.\n");
    }
}

double calculateSubtotal(double price, int noOfTickets) {
    return price * noOfTickets;
}

double calculateDiscount(double subtotal, int noOfTickets) {
    if (noOfTickets >= DISCOUNT_MIN_TICKETS) {
        return subtotal * DISCOUNT_RATE;
    }
    return 0;
}

double calculateFinalBill(double subtotal, double discount) {
    return subtotal - discount;
}

void showOutput(const struct Customer *customer, const char *categoryName,
                double subtotal, double discount, double finalBill) {
    printf("Customer: %s\n", customer->name);
    printf("Ticket Category: %s\n", categoryName);
    printf("Tickets: %d\n", customer->noOfTickets);
    printf("Subtotal: %.2f\n", subtotal);
    printf("Discount: %.2f\n", discount);
    printf("Final Bill: %.2f\n", finalBill);
}

int main() {
    struct Customer customer;
    struct CategoryInfo info;

    enum InputStatus status = takeInputs(&customer);
    if (status != INPUT_OK) {
        showError(status);
        return 1;
    }

    getCategoryInfo(customer.categoryChoice, &info);

    double subtotal = calculateSubtotal(info.price, customer.noOfTickets);
    double discount = calculateDiscount(subtotal, customer.noOfTickets);
    double finalBill = calculateFinalBill(subtotal, discount);

    showOutput(&customer, info.name, subtotal, discount, finalBill);
    return 0;
}
