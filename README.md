# CanteenLedger

CanteenLedger is a C program for managing a canteen's students, wallets, menu and
daily stock. You can place purchases and preorders, process refunds, and view
orders and transaction history from the terminal.

Everything is saved locally in `canteen.db` using SQLite. The program creates
the database on first use and loads your saved records when you open it again.

## Build and run

You need GCC with C11 support and the SQLite development library. On Windows,
install SQLite from an MSYS2 UCRT64 terminal:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-sqlite3
```

Open PowerShell in the project folder, then compile and run:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow main.c ledger.c -l:libsqlite3.a -o canteenledger.exe
.\canteenledger.exe
```

The Windows build includes SQLite in the executable. On Linux, install GCC and
the SQLite development package, then use:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow main.c ledger.c -lsqlite3 -o canteenledger
./canteenledger
```

## Using the program

Running the program in a terminal opens the numbered menu. Choose a section
and follow the prompts. Enter `0` to go back or exit from the main menu.
Orders show their total before you confirm them.

You can also select an interface yourself:

```powershell
.\canteenledger.exe --menu
.\canteenledger.exe --cli
```

Piped input automatically uses command mode. Both interfaces work with the same
database and save each successful change as you go.

Dates use `YYYY-MM-DD`, and amounts are whole currency units. Names can contain
up to 63 bytes. Use underscores between words in command mode; the numbered
menu converts spaces for you.

## Commands

```text
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
```

`student` creates a student wallet, and `topup` adds money to it. `item` adds or
updates a menu item; set `ACTIVE` to `1` to allow orders or `0` to stop them.
`stock` sets the remaining quantity for an item on a particular date.

Orders for today are fulfilled immediately. Future orders charge the wallet
and reserve stock for the target date. Each order keeps the price it was placed
at, even when the menu price changes later.

Cancelling a confirmed preorder refunds its original charge and releases the
reserved stock. Fulfil a preorder on or after its target date. Overdue preorders
stay reserved until you process them.

For `report`, use `0` for all students or items and `*` for all dates or statuses.
Status values are `0` for confirmed, `1` for fulfilled and `2` for cancelled.
Reports show orders, wallet balances and stock availability. `history` shows
saved transactions, and `save` flushes the database cache.

Here is a short command session for a fresh database:

```text
student 1 1000 Ada
item 7 20 1 Rice
stock 7 2026-11-18 10
stock 7 2026-11-19 5
order 1 7 2 2026-11-18 2026-11-18
order 1 7 3 2026-11-18 2026-11-19
report 0 0 * *
history
save
exit
```

## Where the data goes

By default, `canteen.db` is created in the directory you run the program from.
To choose another location, create a folder and pass it with `--data`:

```powershell
New-Item -ItemType Directory -Force my-data | Out-Null
.\canteenledger.exe --data my-data
```

The database stores students, menu items, inventory, orders and transaction
history. SQL values use bound parameters, and each change is saved in a single
transaction. Close the program before copying `canteen.db` for a backup.

## Project files

| File | Purpose |
| --- | --- |
| `main.c` | Numbered menus, prompts and text commands |
| `ledger.c` | Wallet and order rules, stock management and SQLite storage |
| `ledger.h` | Shared records and function declarations |
