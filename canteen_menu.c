#include "canteen_menu.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>

static void heading(const char *title)
{
    printf("\n==================================================\n%s\n"
           "==================================================\n", title);
}

static int readtext(const char *prompt, char *text, size_t size)
{
    for(;;)
    {
        if(fputs(prompt, stdout) == EOF || fflush(stdout) != 0)
        {
            return 0;
        }
        int result = readline(stdin, text, size);
        if(result == 0 || ferror(stdin))
        {
            return 0;
        }
        if(result < 0)
        {
            puts("Invalid input or line too long. Try again.");
            continue;
        }
        size_t start = 0, length = strlen(text);
        while(start < length && isspace((unsigned char)text[start]))
        {
            start++;
        }
        while(length > start && isspace((unsigned char)text[length - 1]))
        {
            length--;
        }
        memmove(text, text + start, length - start);
        text[length - start] = '\0';
        if(text[0])
        {
            return 1;
        }
        puts("Please enter a value.");
    }
}

static int readint(const char *prompt, int minimum, int maximum, int *value)
{
    char text[64];
    while(readtext(prompt, text, sizeof(text)))
    {
        if(number(text, value) && *value >= minimum && *value <= maximum)
        {
            return 1;
        }
        printf("Enter a whole number from %d to %d.\n", minimum, maximum);
    }
    return 0;
}

static int readdate(const char *prompt, char *date)
{
    while(readtext(prompt, date, 11))
    {
        if(validdate(date))
        {
            return 1;
        }
        puts("Enter a valid date (YYYY-MM-DD).");
    }
    return 0;
}

static int readname(const char *prompt, char *name)
{
    while(readtext(prompt, name, 64))
    {
        int valid = 1;
        for(char *p = name; *p; p++)
        {
            if(*p == ' ')
            {
                *p = '_';
            }
            else if(isspace((unsigned char)*p) || iscntrl((unsigned char)*p))
            {
                valid = 0;
            }
        }
        if(valid)
        {
            return 1;
        }
        puts("Names cannot contain tabs or control characters.");
    }
    return 0;
}

static int confirm(const char *prompt)
{
    char text[8];
    while(readtext(prompt, text, sizeof(text)))
    {
        if(strcmp(text, "y") == 0 || strcmp(text, "Y") == 0)
        {
            return 1;
        }
        if(strcmp(text, "n") == 0 || strcmp(text, "N") == 0)
        {
            puts("Cancelled. No changes made.");
            return 0;
        }
        puts("Enter Y or N.");
    }
    return 0;
}

static Status commit(Ledger *ledger, Storage *storage, const char *command)
{
    Status result = storage_commit(storage, ledger, command);
    puts(statusmessage(result));
    return result;
}

static void showstudent(const Ledger *ledger)
{
    int id;
    size_t index;
    if(!readint("Student ID: ", 1, INT_MAX, &id)) return;
    if(!hash_find(&ledger->student_index, id, &index))
    {
        puts("Student not found.");
        return;
    }
    const Student *student = &ledger->students[index];
    printf("Student ID : %d\nName       : %s\nBalance    : %d\n",
           student->id, student->name, student->balance);
}

static void students(Ledger *ledger, Storage *storage)
{
    int choice, id, balance;
    char name[64], command[LINE_SIZE];
    while(!storage->poisoned)
    {
        heading("STUDENT MANAGEMENT");
        puts("1. Add Student\n2. Search Student by ID\n3. Display All Students\n0. Back");
        if(!readint("Enter choice: ", 0, 3, &choice) || choice == 0) return;
        if(choice == 1)
        {
            if(!readint("Student ID: ", 1, INT_MAX, &id) ||
               !readname("Student Name: ", name) ||
               !readint("Initial Wallet Balance: ", 0, INT_MAX, &balance)) return;
            snprintf(command, sizeof(command), "student %d %d %s", id, balance, name);
            commit(ledger, storage, command);
        }
        else if(choice == 2)
        {
            showstudent(ledger);
        }
        else
        {
            printf("%-10s %-63s %s\n", "ID", "Name", "Balance");
            for(size_t i = 0; i < ledger->studentcount; i++)
            {
                const Student *s = &ledger->students[i];
                printf("%-10d %-63s %d\n", s->id, s->name, s->balance);
            }
        }
    }
}

static void menuitems(Ledger *ledger, Storage *storage)
{
    int choice, id, price, active;
    char name[64], command[LINE_SIZE];
    while(!storage->poisoned)
    {
        heading("MENU MANAGEMENT");
        puts("1. Add Menu Item\n2. Modify Menu Item\n3. Display Menu\n"
             "4. Activate/Deactivate Item\n0. Back");
        if(!readint("Enter choice: ", 0, 4, &choice) || choice == 0) return;
        if(choice == 3)
        {
            printf("%-10s %-63s %-10s %s\n", "ID", "Name", "Price", "Active");
            for(size_t i = 0; i < ledger->menucount; i++)
            {
                const Item *item = &ledger->items[i];
                printf("%-10d %-63s %-10d %s\n", item->id, item->name,
                       item->price, item->active ? "Yes" : "No");
            }
            continue;
        }
        if(!readint("Item ID: ", 1, INT_MAX, &id)) return;
        size_t index;
        int exists = hash_find(&ledger->item_index, id, &index);
        if((choice == 1 && exists) || (choice != 1 && !exists))
        {
            puts(exists ? "Item already exists. Use Modify Menu Item." : "Item not found.");
            continue;
        }
        if(choice == 4)
        {
            strcpy(name, ledger->items[index].name);
            price = ledger->items[index].price;
        }
        else if(!readname("Item Name: ", name) ||
                !readint("Price: ", 0, INT_MAX, &price)) return;
        if(!readint("Active (1=Yes, 0=No): ", 0, 1, &active)) return;
        snprintf(command, sizeof(command), "item %d %d %d %s", id, price, active, name);
        commit(ledger, storage, command);
    }
}

static void showinventory(const Ledger *ledger, int item, const char *date)
{
    printf("%-10s %-12s %-10s %-10s %s\n", "Item ID", "Date", "Remaining", "Reserved", "Available");
    for(size_t i = 0; i < ledger->inventorycount; i++)
    {
        const Inventory *v = &ledger->inventory[i];
        if((item == 0 || item == v->item) &&
           (strcmp(date, "*") == 0 || strcmp(date, v->date) == 0))
        {
            printf("%-10d %-12s %-10d %-10d %d\n", v->item, v->date,
                   v->stock, v->reserved, v->stock - v->reserved);
        }
    }
}

static void inventory(Ledger *ledger, Storage *storage)
{
    int choice, id, quantity;
    char date[11], command[LINE_SIZE];
    while(!storage->poisoned)
    {
        heading("INVENTORY MANAGEMENT");
        puts("1. Set Daily Stock\n2. View Inventory by Date\n3. View Item Availability\n0. Back");
        if(!readint("Enter choice: ", 0, 3, &choice) || choice == 0) return;
        id = 0;
        if(choice != 2 && !readint("Item ID: ", 1, INT_MAX, &id)) return;
        if(!readdate("Date (YYYY-MM-DD): ", date)) return;
        if(choice == 1)
        {
            if(!readint("Stock Quantity: ", 0, INT_MAX, &quantity)) return;
            snprintf(command, sizeof(command), "stock %d %s %d", id, date, quantity);
            commit(ledger, storage, command);
        }
        else showinventory(ledger, id, date);
    }
}

static void place(Ledger *ledger, Storage *storage, int preorder)
{
    int student, item, quantity;
    char today[11], target[11], command[LINE_SIZE];
    heading(preorder ? "PLACE PREORDER" : "PLACE IMMEDIATE ORDER");
    if(!readint("Student ID: ", 1, INT_MAX, &student) ||
       !readint("Item ID: ", 1, INT_MAX, &item) ||
       !readint("Quantity: ", 1, INT_MAX, &quantity) ||
       !readdate("Today's Date (YYYY-MM-DD): ", today)) return;
    strcpy(target, today);
    if(preorder)
    {
        do
        {
            if(!readdate("Target Date (YYYY-MM-DD): ", target)) return;
            if(strcmp(target, today) > 0) break;
            puts("Target date must be later than today's date.");
        } while(1);
    }
    size_t index;
    if(!hash_find(&ledger->item_index, item, &index))
    {
        puts("Item not found.");
        return;
    }
    int price = ledger->items[index].price;
    if(price && quantity > INT_MAX / price)
    {
        puts("overflow");
        return;
    }
    printf("Student ID : %d\nItem ID    : %d\nQuantity   : %d\n"
           "Unit Price : %d\nTotal      : %d\nTarget Date: %s\n",
           student, item, quantity, price, quantity * price, target);
    if(!confirm("Confirm order? (Y/N): ")) return;
    snprintf(command, sizeof(command), "order %d %d %d %s %s",
             student, item, quantity, today, target);
    if(commit(ledger, storage, command) == OK)
    {
        const Order *order = &ledger->orders[ledger->ordercount - 1];
        printf("Order ID: %d  Status: %s\n", order->id,
               order->state == CONFIRMED ? "CONFIRMED" : "FULFILLED");
    }
}

static int readstatus(int *status)
{
    char text[32];
    while(readtext("Status (CONFIRMED/FULFILLED/CANCELLED, *=all): ", text, sizeof(text)))
    {
        for(char *p = text; *p; p++) *p = (char)toupper((unsigned char)*p);
        if(strcmp(text, "*") == 0) { *status = -1; return 1; }
        if(strcmp(text, "CONFIRMED") == 0) { *status = CONFIRMED; return 1; }
        if(strcmp(text, "FULFILLED") == 0) { *status = FULFILLED; return 1; }
        if(strcmp(text, "CANCELLED") == 0) { *status = CANCELLED; return 1; }
        puts("Unknown status.");
    }
    return 0;
}

static int filterdate(char *date)
{
    while(readtext("Target date (YYYY-MM-DD, *=all): ", date, 11))
    {
        if(strcmp(date, "*") == 0 || validdate(date)) return 1;
        puts("Enter a valid date or *.");
    }
    return 0;
}

static void showorders(const Ledger *ledger, int student, int item,
                       const char *date, int status, int preorders)
{
    puts("Orders: ID, student, item, quantity, unit price, total, placed date, target date, status");
    reportorders(ledger, stdout, student, item, date, status, preorders);
}

static void preorders(Ledger *ledger, Storage *storage)
{
    int choice, id, student, item, status;
    char today[11], date[11], command[LINE_SIZE];
    while(!storage->poisoned)
    {
        heading("PREORDER MANAGEMENT");
        puts("1. Place Preorder\n2. Cancel Preorder\n3. Fulfil Preorder\n4. View Preorders\n0. Back");
        if(!readint("Enter choice: ", 0, 4, &choice) || choice == 0) return;
        if(choice == 1) place(ledger, storage, 1);
        else if(choice == 4)
        {
            if(!readint("Student ID (0=all): ", 0, INT_MAX, &student) ||
               !readint("Item ID (0=all): ", 0, INT_MAX, &item) ||
               !filterdate(date) || !readstatus(&status)) return;
            showorders(ledger, student, item, date, status, 1);
        }
        else
        {
            if(!readint("Order ID: ", 1, INT_MAX, &id) ||
               !readdate("Today's Date (YYYY-MM-DD): ", today)) return;
            if((size_t)id > ledger->ordercount)
            {
                puts("Order not found.");
                continue;
            }
            const Order *o = &ledger->orders[id - 1];
            printf("Order %d: student=%d item=%d qty=%d total=%d due=%s status=%s\n",
                   o->id, o->student, o->item, o->quantity, o->total, o->date,
                   o->state == CONFIRMED ? "CONFIRMED" :
                   o->state == FULFILLED ? "FULFILLED" : "CANCELLED");
            if(choice == 2 && !confirm("Cancel this preorder? (Y/N): ")) continue;
            snprintf(command, sizeof(command), "%s %d %s",
                     choice == 2 ? "cancel" : "fulfil", id, today);
            commit(ledger, storage, command);
        }
    }
}

static void wallets(Ledger *ledger, Storage *storage)
{
    int choice, id, amount;
    char command[LINE_SIZE];
    while(!storage->poisoned)
    {
        heading("WALLET MANAGEMENT");
        puts("1. Add Money\n2. Check Balance\n0. Back");
        if(!readint("Enter choice: ", 0, 2, &choice) || choice == 0) return;
        if(choice == 2) showstudent(ledger);
        else
        {
            if(!readint("Student ID: ", 1, INT_MAX, &id) ||
               !readint("Amount: ", 1, INT_MAX, &amount)) return;
            snprintf(command, sizeof(command), "topup %d %d", id, amount);
            commit(ledger, storage, command);
        }
    }
}

static void reports(const Ledger *ledger)
{
    int choice;
    for(;;)
    {
        heading("REPORTS");
        puts("1. All Orders\n2. Orders by Student\n3. Orders by Item\n"
             "4. Orders by Date\n5. Orders by Status\n6. Inventory Summary\n0. Back");
        if(!readint("Enter choice: ", 0, 6, &choice) || choice == 0) return;
        int student = 0, item = 0, status = -1;
        char date[11] = "*";
        if(choice == 2 && !readint("Student ID: ", 1, INT_MAX, &student)) return;
        if(choice == 3 && !readint("Item ID: ", 1, INT_MAX, &item)) return;
        if(choice == 4 && !readdate("Target Date (YYYY-MM-DD): ", date)) return;
        if(choice == 5 && !readstatus(&status)) return;
        if(choice == 6) showinventory(ledger, 0, "*");
        else showorders(ledger, student, item, date, status, 0);
    }
}

void runmenu(Ledger *ledger, Storage *storage)
{
    int choice;
    while(!storage->poisoned && !feof(stdin) && !ferror(stdin) && !ferror(stdout))
    {
        heading("CANTEENLEDGER\nCanteen Management System");
        puts("1. Student Management\n2. Menu Management\n3. Inventory Management\n"
             "4. Place Immediate Order\n5. Preorder Management\n6. Wallet Management\n"
             "7. Reports\n8. Transaction History\n9. Save Data\n0. Exit");
        if(!readint("Enter your choice: ", 0, 9, &choice) || choice == 0) return;
        switch(choice)
        {
            case 1: students(ledger, storage); break;
            case 2: menuitems(ledger, storage); break;
            case 3: inventory(ledger, storage); break;
            case 4: place(ledger, storage, 0); break;
            case 5: preorders(ledger, storage); break;
            case 6: wallets(ledger, storage); break;
            case 7: reports(ledger); break;
            case 8:
                heading("TRANSACTION HISTORY");
                puts("Saved sequence IDs and commands (with checksums and commit markers):");
                displayhistory(storage);
                break;
            case 9: puts(statusmessage(storage_checkpoint(storage))); break;
        }
    }
}
