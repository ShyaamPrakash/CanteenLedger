#ifndef LEDGER_H
#define LEDGER_H
#include "hash_table.h"
#include <stdio.h>
#include <stdint.h>
#define LINE_SIZE 512
typedef enum
{
    OK, INVALID, DUPLICATE, MISSING, FUNDS, STOCK, OVERFLOW, MEMORY, IO_ERROR, CORRUPT, TRANSITION
} Status;
typedef struct
{
    int id;
    char name[64];
    int balance;
} Student;
typedef struct
{
    int id;
    char name[64];
    int price,active;
} Item;
typedef struct
{
    int item;
    char date[11];
    int stock,reserved;
} Inventory;
typedef enum
{
    CONFIRMED, FULFILLED, CANCELLED
} OrderState;
typedef struct
{
    int id,student,item,quantity,unit_price,total;
    char placed[11],date[11];
    OrderState state;
} Order;
/* Ledger owns the record arrays and indexes. Free it with freeledger(). */
typedef struct
{
    Student *students;
    size_t studentcount,studentcapacity;
    Item *items;
    size_t menucount,menucapacity;
    Inventory *inventory;
    size_t inventorycount,inventorycapacity;
    Order *orders;
    size_t ordercount,ordercapacity;
    Hash student_index,item_index;
} Ledger;
void freeledger(Ledger *ledger);
Status copyledger(const Ledger *ledger,Ledger *out);
void *grow(void *records, size_t *capacity, size_t count, size_t width);
int validdate(const char *s);
int number(const char *s,int *out);
/* Returns 1 for LF, 2 for a final line without LF, 0 for EOF, -1 for error. */
int readline(FILE *f,char *s,size_t size);
const char *statusmessage(Status s);
Status applycommand(Ledger *ledger,const char *command);
Inventory *findinventory(Ledger *ledger,int item,const char *date);
Status addstudent(Ledger *ledger,int id,const char *name,int balance);
Status addbalance(Ledger *ledger,int id,int amount);
Status setmenuitem(Ledger *ledger,int id,const char *name,int price,int active);
Status setinventory(Ledger *ledger,int id,const char *date,int stock);
Status placeorder(Ledger *ledger,int student,int item,int quantity,const char *today,const char *date);
Status finishorder(Ledger *ledger,int id,const char *today,int cancel);
void report(const Ledger *ledger,FILE *f,int student,int item,const char *date,int state);
void reportorders(const Ledger *ledger,FILE *f,int student,int item,const char *date,int state,int preorders);
typedef struct
{
    char path[1024];
    unsigned long sequence;
    int poisoned;
} Storage;
Status storage_open(Storage *s,Ledger *ledger,const char *directory);
Status storage_commit(Storage *s,Ledger *ledger,const char *command);
Status storage_checkpoint(Storage *s);
#endif
