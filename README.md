# CanteenLedger

CanteenLedger is a terminal program for managing a canteen's students, wallet
balances, menu and stock. It records immediate purchases and future preorders,
with cancellation, refunds and fulfilment.

Amounts are whole integer currency units. Dates use YYYY-MM-DD. Names can contain
up to 63 bytes; use underscores between words.

## Build and run

You need GCC with C11 support. Open PowerShell in the project folder and compile:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow canteen_cli.c canteen_menu.c canteen_ledger.c -o canteenledger.exe
```

Create a folder for your data, then start the program:

```powershell
New-Item -ItemType Directory -Force my-data
.\canteenledger.exe --data my-data
```

The data directory must already exist. Without --data, the program uses the
current directory. Each successful change is saved automatically.

## Interface Modes

### Interactive Menu

```powershell
.\canteenledger.exe --menu --data my-data
```

Choose a number and follow the prompts:

```text
==================================================
CANTEENLEDGER
Canteen Management System
==================================================
1. Student Management
2. Menu Management
3. Inventory Management
4. Place Immediate Order
5. Preorder Management
6. Wallet Management
7. Reports
8. Transaction History
9. Save Data
0. Exit
Enter your choice:
```

Submenus use 0 to go back. Orders show a total before asking for confirmation.
Spaces in names are saved as underscores to match the command format. Reports
accept status names such as CONFIRMED, FULFILLED and CANCELLED.

### Command Mode

```powershell
.\canteenledger.exe --cli --data my-data
```

Enter one command per line. For example:

```text
> student 101 500 Shyaam
OK
> topup 101 100
OK
> report 101 0 * *
student 101 Shyaam balance=600
> exit
```

### Automatic Mode Selection

Terminal input opens the menu by default. Piped or redirected input uses command
mode. The --menu and --cli flags override this choice; using both is an error.
Use --help for launch options. Choose 0 from the main menu, type exit in command
mode, or send EOF to leave. EOF during a prompt abandons the unfinished action.

Both modes read and save the same ledger.log in the selected data folder.

## Commands

    student ID BALANCE NAME
    topup STUDENT AMOUNT
    item ID PRICE ACTIVE NAME
    stock ITEM YYYY-MM-DD QUANTITY
    order STUDENT ITEM QUANTITY TODAY TARGET
    cancel ORDER TODAY
    fulfil ORDER TODAY
    report STUDENT ITEM DATE STATUS
    history
    save
    help
    exit

student adds a student; duplicate IDs are rejected. topup adds a positive amount.
item creates a menu item or changes its name, price and active status. Set ACTIVE
to 1 to allow orders or 0 to stop new orders.

stock sets the remaining stock for one item and date. It cannot go below the
quantity already reserved. Missing stock for a future date causes an order to fail.

An order with the same TODAY and TARGET dates is fulfilled immediately. A future
order charges the wallet and reserves stock for its target date. Prices are
captured when the order is placed, so later menu changes do not change its total.

You can cancel a confirmed preorder until it is fulfilled, even if it is overdue.
Cancellation releases its reservation and refunds its original charge once.
Fulfilment is allowed on or after the target date and consumes the reservation
once. Overdue orders stay reserved until you process them. If a refund would
overflow the wallet balance, cancellation fails without changing the order.

For report, use 0 for all students or items, and * for all dates or statuses.
Status values are 0 for CONFIRMED, 1 for FULFILLED and 2 for CANCELLED.
Student, item, date and status filters apply together to order rows. Catalog rows
filter their own IDs; inventory totals filter by item and date.

Reports show remaining stock, reservations, availability and fulfilled quantities.
The planned total is remaining stock plus fulfilled quantities for the current
plan. Accepted orders cannot exceed stock, so the reported shortage is zero.
history displays the transaction log with its sequence IDs.

## Files

The main files are:

- canteen_cli.c selects the interface and handles text commands and history.
- canteen_menu.c handles numbered menus and guided input.
- canteen_menu.h declares the menu and shared history display.
- canteen_ledger.c contains validation, wallet and order rules, inventory, hash lookup
  and file storage.
- ledger.h declares the records and ledger functions.
- hash_table.h declares the hash index.

## Storage and limits

The program stores a versioned ledger.log in the chosen data directory. Each
change records its command, checksum, sequence ID and commit marker.
It checks writes, flushes the file and requests an OS sync before updating the
live in-memory state.

Startup reads committed commands into a fresh ledger. An incomplete final record
is removed; a malformed complete record causes an error. A failed append can leave
its disk outcome uncertain, so the program exits and requires a restart.

save writes the full log to a temporary file and replaces the old log atomically.
It keeps the full history and does not reduce the log's size.

Run one instance per data directory. The program has no authentication, encryption
or concurrent-writer lock. The operator supplies today's date. Parent-directory
sync is absent, and power-loss recovery has not been verified. Keep backups of
important data.

Old students.txt, menu.txt and preorders.txt files are not imported automatically.
The program uses its versioned ledger.log format.

