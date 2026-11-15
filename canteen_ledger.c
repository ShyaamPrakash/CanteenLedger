#define _POSIX_C_SOURCE 200809L
#include "ledger.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
void *grow(void *records, size_t *capacity, size_t count, size_t width)
{
    if(width == 0 || count == SIZE_MAX)
    {
        return NULL;
    }
    if(count < *capacity)
    {
        return records;
    }
    if(*capacity > SIZE_MAX / 2)
    {
        return NULL;
    }

    size_t newcapacity = *capacity ? *capacity * 2 : 16;
    if(newcapacity <= count)
    {
        newcapacity = count + 1;
    }
    if(newcapacity > SIZE_MAX / width)
    {
        return NULL;
    }

    void *updated = realloc(records, newcapacity * width);
    if(updated == NULL)
    {
        return NULL;
    }
    *capacity = newcapacity;
    return updated;
}

int number(const char *s,int *out)
{
    if(!s || !*s)
    {
        return 0;
    }
    for(const char *p=s;*p;p++)
    {
        if(*p<'0'||*p>'9')
        {
            return 0;
        }
    }
    errno=0;
    char *end;
    long n=strtol(s,&end,10);
    if(errno || *end || n>INT_MAX)
    {
        return 0;
    }
    *out=(int)n;
    return 1;
}

int validdate(const char *s)
{
    if(strlen(s)!=10 || s[4]!='-' || s[7]!='-')
    {
        return 0;
    }
    for(int i=0;i<10;i++)
    {
        if(i!=4 && i!=7 && (s[i]<'0'||s[i]>'9'))
        {
            return 0;
        }
    }
    int y=(s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+s[3]-'0';
    int m=(s[5]-'0')*10+s[6]-'0', d=(s[8]-'0')*10+s[9]-'0';
    const int days[]=
    {
        31,28,31,30,31,30,31,31,30,31,30,31
    };
    return y>=1 && m>=1 && m<=12 && d>=1 && d<=days[m-1]+(m==2 && (y%400==0 || (y%4==0 && y%100!=0)));
}

int readline(FILE *fp, char *line, size_t size)
{
    if(fp == NULL || line == NULL || size < 2)
    {
        return -1;
    }

    size_t count = 0;
    int invalid = 0;
    int ch;
    while((ch = fgetc(fp)) != EOF)
    {
        if(ch == '\n')
        {
            if(count > 0 && line[count - 1] == '\r')
            {
                count--;
            }
            line[count] = '\0';
            return invalid ? -1 : 1;
        }
        if(ch == 0 || count == size - 1)
        {
            invalid = 1;
        }
        else if(!invalid)
        {
            line[count++] = (char)ch;
        }
    }
    line[count] = '\0';
    if(ferror(fp) || invalid)
    {
        return -1;
    }
    /* A terminal command may end at EOF; a journal commit must end with LF. */
    return count > 0 ? 2 : 0;
}

const char *statusmessage(Status s)
{
    static const char *names[]=
    {
        "OK","invalid input","duplicate ID","missing reference","insufficient balance","insufficient stock","overflow","allocation failed","I/O error","corrupt data","invalid transition"
    };
    return names[s];
}

void freeledger(Ledger *ledger)
{
    free(ledger->students);
    free(ledger->items);
    free(ledger->inventory);
    free(ledger->orders);
    hash_free(&ledger->student_index);
    hash_free(&ledger->item_index);
    *ledger=(Ledger){0};
}

static void *copy_records(const void *records, size_t count, size_t width)
{
    if(count == 0 || width == 0 || count > SIZE_MAX / width)
    {
        return NULL;
    }
    void *copy = malloc(count * width);
    if(copy != NULL)
    {
        memcpy(copy, records, count * width);
    }
    return copy;
}

Status copyledger(const Ledger *original, Ledger *copy)
{
    if(original == copy)
    {
        return INVALID;
    }
    *copy = (Ledger){0};
    copy->students = copy_records(original->students, original->studentcount, sizeof(Student));
    copy->items = copy_records(original->items, original->menucount, sizeof(Item));
    copy->inventory = copy_records(original->inventory, original->inventorycount, sizeof(Inventory));
    copy->orders = copy_records(original->orders, original->ordercount, sizeof(Order));

    if((original->studentcount && copy->students == NULL) ||
       (original->menucount && copy->items == NULL) ||
       (original->inventorycount && copy->inventory == NULL) ||
       (original->ordercount && copy->orders == NULL))
    {
        freeledger(copy);
        return MEMORY;
    }

    copy->studentcount = copy->studentcapacity = original->studentcount;
    copy->menucount = copy->menucapacity = original->menucount;
    copy->inventorycount = copy->inventorycapacity = original->inventorycount;
    copy->ordercount = copy->ordercapacity = original->ordercount;
    for(size_t i = 0; i < copy->studentcount; i++)
    {
        if(!hash_put(&copy->student_index, copy->students[i].id, i))
        {
            freeledger(copy);
            return MEMORY;
        }
    }
    for(size_t i = 0; i < copy->menucount; i++)
    {
        if(!hash_put(&copy->item_index, copy->items[i].id, i))
        {
            freeledger(copy);
            return MEMORY;
        }
    }
    return OK;
}

static size_t bucket(int key,size_t n)
{
    return ((uint32_t)key * UINT32_C(2654435761)) % n;
}

void hash_free(Hash *h)
{
    for(size_t i=0;i<h->capacity;i++)
    {
        Node *p=h->buckets[i];
        while(p)
        {
            Node *q=p->next;
            free(p);
            p=q;
        }
    }
    free(h->buckets);
    *h=(Hash){0};
}

int hash_find(const Hash *hash, int key, size_t *value)
{
    if(hash->capacity == 0)
    {
        return 0;
    }
    Node *entry = hash->buckets[bucket(key, hash->capacity)];
    while(entry != NULL)
    {
        if(entry->key == key)
        {
            if(value != NULL)
            {
                *value = entry->value;
            }
            return 1;
        }
        entry = entry->next;
    }
    return 0;
}

int hash_put(Hash *h,int key,size_t value)
{
    if(hash_find(h,key,NULL))
    {
        return 0;
    }
    if(!h->capacity || h->count>=h->capacity/2)
    {
        if(h->capacity>SIZE_MAX/2/sizeof(Node*))
        {
            return 0;
        }
        size_t n=h->capacity?h->capacity*2:16;
        Node **b=calloc(n,sizeof(*b));
        if(!b)
        {
            return 0;
        }
        for(size_t i=0;i<h->capacity;i++)
        {
            Node *p=h->buckets[i];
            while(p)
            {
                Node *q=p->next;
                size_t j=bucket(p->key,n);
                p->next=b[j];
                b[j]=p;
                p=q;
            }
        }
        free(h->buckets);
        h->buckets=b;
        h->capacity=n;
    }
    Node *p=malloc(sizeof(*p));
    if(!p)
    {
        return 0;
    }
    size_t j=bucket(key,h->capacity);
    p->key = key;
    p->value = value;
    p->next = h->buckets[j];
    h->buckets[j]=p;
    h->count++;
    return 1;
}

int hash_remove(Hash *h,int key)
{
    if(!h->capacity)
    {
        return 0;
    }
    Node **p=&h->buckets[bucket(key,h->capacity)];
    while(*p)
    {
        if((*p)->key==key)
        {
            Node *q=*p;
            *p=q->next;
            free(q);
            h->count--;
            return 1;
        }
        p=&(*p)->next;
    }
    return 0;
}

Status addstudent(Ledger *ledger,int id,const char *name,int balance)
{
    if(id<=0 || balance<0 || !*name || strlen(name)>=64)
    {
        return INVALID;
    }
    if(hash_find(&ledger->student_index,id,NULL))
    {
        return DUPLICATE;
    }
    Student *students = grow(ledger->students, &ledger->studentcapacity, ledger->studentcount, sizeof(Student));
    if(students == NULL)
    {
        return MEMORY;
    }
    ledger->students = students;
    if(!hash_put(&ledger->student_index,id,ledger->studentcount))
    {
        return MEMORY;
    }
    Student *s=&ledger->students[ledger->studentcount++];
    s->id = id;
    s->balance = balance;
    strcpy(s->name,name);
    return OK;
}

Status addbalance(Ledger *ledger,int id,int amount)
{
    size_t i;
    if(amount<=0)
    {
        return INVALID;
    }
    if(!hash_find(&ledger->student_index,id,&i))
    {
        return MISSING;
    }
    if(ledger->students[i].balance>INT_MAX-amount)
    {
        return OVERFLOW;
    }
    ledger->students[i].balance+=amount;
    return OK;
}

Status setmenuitem(Ledger *ledger,int id,const char *name,int price,int active)
{
    if(id<=0 || price<0 || !*name || strlen(name)>=64 || (active!=0 && active!=1))
    {
        return INVALID;
    }
    size_t i;
    if(!hash_find(&ledger->item_index,id,&i))
    {
        Item *items = grow(ledger->items, &ledger->menucapacity,
                           ledger->menucount, sizeof(Item));
        if(items == NULL)
        {
            return MEMORY;
        }
        ledger->items = items;
        i=ledger->menucount;
        if(!hash_put(&ledger->item_index,id,i))
        {
            return MEMORY;
        }
        ledger->menucount++;
    }
    ledger->items[i].id = id;
    ledger->items[i].price = price;
    ledger->items[i].active = active;
    strcpy(ledger->items[i].name,name);
    return OK;
}

Inventory *findinventory(Ledger *ledger,int item,const char *date)
{
    for(size_t i=0;i<ledger->inventorycount;i++)
    {
        if(ledger->inventory[i].item==item && !strcmp(ledger->inventory[i].date,date))
        {
            return &ledger->inventory[i];
        }
    }
    return NULL;
}

Status setinventory(Ledger *ledger,int id,const char *date,int stock)
{
    if(stock<0 || !validdate(date))
    {
        return INVALID;
    }
    if(!hash_find(&ledger->item_index,id,NULL))
    {
        return MISSING;
    }
    Inventory *v=findinventory(ledger,id,date);
    if(v)
    {
        if(stock<v->reserved)
        {
            return STOCK;
        }
        v->stock=stock;
        return OK;
    }
    Inventory *inventory = grow(ledger->inventory, &ledger->inventorycapacity, ledger->inventorycount, sizeof(Inventory));
    if(inventory == NULL)
    {
        return MEMORY;
    }
    ledger->inventory = inventory;
    v=&ledger->inventory[ledger->inventorycount++];
    v->item = id;
    v->stock = stock;
    v->reserved = 0;
    strcpy(v->date,date);
    return OK;
}

Status placeorder(Ledger *ledger,int student,int item,int quantity,const char *today,const char *date)
{
    if(quantity<=0 || !validdate(today) || !validdate(date) || strcmp(date,today)<0)
    {
        return INVALID;
    }
    size_t si,ii;
    if(!hash_find(&ledger->student_index,student,&si) || !hash_find(&ledger->item_index,item,&ii))
    {
        return MISSING;
    }
    Item *m=&ledger->items[ii];
    if(!m->active)
    {
        return INVALID;
    }
    if(m->price && quantity>INT_MAX/m->price)
    {
        return OVERFLOW;
    }
    int total=quantity*m->price;
    Inventory *v=findinventory(ledger,item,date);
    if(!v || v->stock-v->reserved<quantity)
    {
        return STOCK;
    }
    if(ledger->students[si].balance<total)
    {
        return FUNDS;
    }
    if(ledger->ordercount>=INT_MAX)
    {
        return OVERFLOW;
    }
    Order *orders = grow(ledger->orders, &ledger->ordercapacity, ledger->ordercount, sizeof(Order));
    if(orders == NULL)
    {
        return MEMORY;
    }
    ledger->orders = orders;
    Order *o=&ledger->orders[ledger->ordercount];
    o->id = (int)ledger->ordercount + 1;
    o->student = student;
    o->item = item;
    o->quantity = quantity;
    o->unit_price = m->price;
    o->total = total;
    if(strcmp(today, date) == 0)
    {
        o->state = FULFILLED;
    }
    else
    {
        o->state = CONFIRMED;
    }
    strcpy(o->placed,today);
    strcpy(o->date,date);
    ledger->ordercount++;
    ledger->students[si].balance-=total;
    if(o->state==CONFIRMED)
    {
        v->reserved+=quantity;
    }
    else
    {
        v->stock-=quantity;
    }
    return OK;
}

Status finishorder(Ledger *ledger,int id,const char *today,int cancel)
{
    if(id<=0 || (size_t)id>ledger->ordercount || !validdate(today))
    {
        return INVALID;
    }
    Order *o=&ledger->orders[id-1];
    if(o->state!=CONFIRMED || strcmp(today,o->placed)<0 || (!cancel && strcmp(today,o->date)<0))
    {
        return TRANSITION;
    }
    size_t si;
    if(!hash_find(&ledger->student_index,o->student,&si))
    {
        return MISSING;
    }
    Inventory *v=findinventory(ledger,o->item,o->date);
    if(!v || v->reserved<o->quantity || v->stock<v->reserved)
    {
        return CORRUPT;
    }
    if(cancel && ledger->students[si].balance>INT_MAX-o->total)
    {
        return OVERFLOW;
    }
    if(cancel)
    {
        ledger->students[si].balance+=o->total;
    }
    else
    {
        v->stock-=o->quantity;
    }
    v->reserved-=o->quantity;
    o->state=cancel?CANCELLED:FULFILLED;
    return OK;
}

void report(const Ledger *ledger, FILE *fp, int studentid, int itemid,
            const char *date, int status)
{
    for(size_t i = 0; i < ledger->studentcount; i++)
    {
        const Student *student = &ledger->students[i];
        if(studentid == 0 || studentid == student->id)
        {
            fprintf(fp, "student %d %s balance=%d\n",
                    student->id, student->name, student->balance);
        }
    }
    for(size_t i = 0; i < ledger->menucount; i++)
    {
        const Item *item = &ledger->items[i];
        if(itemid == 0 || itemid == item->id)
        {
            fprintf(fp, "item %d %s price=%d active=%d\n",
                    item->id, item->name, item->price, item->active);
        }
    }
    for(size_t i = 0; i < ledger->inventorycount; i++)
    {
        const Inventory *inventory = &ledger->inventory[i];
        if((itemid != 0 && itemid != inventory->item) ||
           (strcmp(date, "*") != 0 && strcmp(date, inventory->date) != 0))
        {
            continue;
        }
        long long fulfilled = 0;
        for(size_t j = 0; j < ledger->ordercount; j++)
        {
            const Order *order = &ledger->orders[j];
            if(order->item == inventory->item &&
               strcmp(order->date, inventory->date) == 0 &&
               order->state == FULFILLED)
            {
                fulfilled += order->quantity;
            }
        }
        fprintf(fp, "inventory %d %s planned=%lld stock=%d reserved=%d "
                    "available=%d fulfilled=%lld shortage=0\n",
                inventory->item, inventory->date,
                (long long)inventory->stock + fulfilled, inventory->stock,
                inventory->reserved, inventory->stock - inventory->reserved,
                fulfilled);
    }
    reportorders(ledger, fp, studentid, itemid, date, status, 0);
}

void reportorders(const Ledger *ledger, FILE *fp, int studentid, int itemid,
                  const char *date, int status, int preorders)
{
    for(size_t i = 0; i < ledger->ordercount; i++)
    {
        const Order *order = &ledger->orders[i];
        if((preorders && strcmp(order->placed, order->date) == 0) ||
           (studentid != 0 && studentid != order->student) ||
           (itemid != 0 && itemid != order->item) ||
           (strcmp(date, "*") != 0 && strcmp(date, order->date) != 0) ||
           (status >= 0 && status != (int)order->state))
        {
            continue;
        }
        const char *orderstatus = "CONFIRMED";
        if(order->state == FULFILLED)
        {
            orderstatus = "FULFILLED";
        }
        else if(order->state == CANCELLED)
        {
            orderstatus = "CANCELLED";
        }
        fprintf(fp, "order %d student=%d item=%d qty=%d unit=%d total=%d "
                    "placed=%s due=%s status=%s\n",
                order->id, order->student, order->item, order->quantity,
                order->unit_price, order->total, order->placed,
                order->date, orderstatus);
    }
}

Status applycommand(Ledger *ledger,const char *command)
{
    if(strlen(command)>=LINE_SIZE || strchr(command,'\n') || strchr(command,'\r'))
    {
        return INVALID;
    }
    char line[LINE_SIZE];
    strcpy(line,command);
    char *t[9];
    size_t n=0;
    char *p=strtok(line," \t");
    while(p)
    {
        if(n==9)
        {
            return INVALID;
        }
        t[n++]=p;
        p=strtok(NULL," \t");
    }
    if(!n)
    {
        return INVALID;
    }
    int a,b,c,d;
    if(!strcmp(t[0],"student") && n==4 && number(t[1],&a) && number(t[2],&b))
    {
        return addstudent(ledger,a,t[3],b);
    }
    if(!strcmp(t[0],"topup") && n==3 && number(t[1],&a) && number(t[2],&b))
    {
        return addbalance(ledger,a,b);
    }
    if(!strcmp(t[0],"item") && n==5 && number(t[1],&a) && number(t[2],&b) && number(t[3],&c))
    {
        return setmenuitem(ledger,a,t[4],b,c);
    }
    if(!strcmp(t[0],"stock") && n==4 && number(t[1],&a) && number(t[3],&b))
    {
        return setinventory(ledger,a,t[2],b);
    }
    if(!strcmp(t[0],"order") && n==6 && number(t[1],&a) && number(t[2],&b) && number(t[3],&c))
    {
        return placeorder(ledger,a,b,c,t[4],t[5]);
    }
    if((!strcmp(t[0],"cancel") || !strcmp(t[0],"fulfil")) && n==3 && number(t[1],&d))
    {
        return finishorder(ledger,d,t[2],!strcmp(t[0],"cancel"));
    }
    return INVALID;
}
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define SYNC(f) _commit(_fileno(f))
#else
#include <unistd.h>
#define SYNC(f) fsync(fileno(f))
#endif
static uint32_t checksum(const char *p)
{
    uint32_t h=UINT32_C(2166136261);
    while(*p)
    {
        h^=(unsigned char)*p++;
        h*=UINT32_C(16777619);
    }
    return h;
}

static int durable_close(FILE *f)
{
    int bad=fflush(f)!=0;
    if(!bad && SYNC(f)!=0)
    {
        bad=1;
    }
    if(fclose(f)!=0)
    {
        bad=1;
    }
    return !bad;
}

static int truncate_at(FILE *f,long end)
{
    if(fflush(f)!=0)
    {
        return 0;
    }
#ifdef _WIN32
    return _chsize_s(_fileno(f),(long long)end)==0 && SYNC(f)==0;
#else
    return ftruncate(fileno(f),(off_t)end)==0 && SYNC(f)==0;
#endif
}

Status storage_open(Storage *s,Ledger *ledger,const char *directory)
{
    *s=(Storage){0};
    if(snprintf(s->path,sizeof(s->path),"%s/ledger.log",directory)>=(int)sizeof(s->path))
    {
        return INVALID;
    }
    FILE *f=fopen(s->path,"r+b");
    if(!f)
    {
        if(errno!=ENOENT)
        {
            return IO_ERROR;
        }
        f=fopen(s->path,"wb");
        if(!f)
        {
            return IO_ERROR;
        }
        if(fputs("CanteenLedger 1\n",f)==EOF)
        {
            fclose(f);
            return IO_ERROR;
        }
        if(!durable_close(f))
        {
            return IO_ERROR;
        }
        freeledger(ledger);
        return OK;
    }
    Ledger temp = {0};
    char line[LINE_SIZE+80],commit[80];
    long good=0;
    Status result=CORRUPT;
    if(readline(f,line,sizeof(line))!=1 || strcmp(line,"CanteenLedger 1"))
    {
        goto done;
    }
    good=ftell(f);
    if(good<0)
    {
        result=IO_ERROR;
        goto done;
    }
    for(;;)
    {
        int r=readline(f,line,sizeof(line));
        if(r==0)
        {
            result=OK;
            break;
        }
        if(r<0 || r==2)
        {
            if(ferror(f))
            {
                result=IO_ERROR;
                goto done;
            }
            if(!feof(f))
            {
                goto done;
            }
            if(!truncate_at(f,good))
            {
                result=IO_ERROR;
                goto done;
            }
            result=OK;
            break;
        }
        r=readline(f,commit,sizeof(commit));
        if(r!=1)
        {
            if(ferror(f))
            {
                result=IO_ERROR;
                goto done;
            }
            if(!feof(f))
            {
                goto done;
            }
            if(!truncate_at(f,good))
            {
                result=IO_ERROR;
                goto done;
            }
            result=OK;
            break;
        }
        char *first=strchr(line,' '), *second=first?strchr(first+1,' '):NULL;
        if(!first || !second || !second[1])
        {
            goto done;
        }
        *first=0;
        *second=0;
        for(char *p=line;*p;p++)
        {
            if(*p<'0'||*p>'9')
            {
                goto done;
            }
        }
        if(!*line || strlen(first+1)!=8)
        {
            goto done;
        }
        for(char *p=first+1;*p;p++)
        {
            if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')))
            {
                goto done;
            }
        }
        errno=0;
        char *end;
        unsigned long seq=strtoul(line,&end,10);
        if(errno || *end || s->sequence==ULONG_MAX || seq!=s->sequence+1)
        {
            goto done;
        }
        errno=0;
        unsigned long hash=strtoul(first+1,&end,16);
        if(errno || *end || hash>UINT32_MAX || checksum(second+1)!=(uint32_t)hash)
        {
            goto done;
        }
        char expected[80];
        snprintf(expected,sizeof(expected),"COMMIT %lu",seq);
        if(strcmp(expected,commit))
        {
            goto done;
        }
        Status replaystatus = applycommand(&temp, second + 1);
        if(replaystatus != OK)
        {
            result = replaystatus == MEMORY ? MEMORY : CORRUPT;
            goto done;
        }
        s->sequence=seq;
        good=ftell(f);
        if(good<0)
        {
            result=IO_ERROR;
            goto done;
        }
    }
done:
    if(fclose(f)!=0)
    {
        result=IO_ERROR;
    }
    if(result==OK)
    {
        freeledger(ledger);
        *ledger=temp;
    }
    else
    {
        freeledger(&temp);
    }
    return result;
}

/* Publish the candidate only after the journal write has been synced. */
Status storage_commit(Storage *s,Ledger *ledger,const char *command)
{
    if(s->poisoned)
    {
        return IO_ERROR;
    }
    if(s->sequence==ULONG_MAX)
    {
        return OVERFLOW;
    }
    Ledger candidate = {0};
    Status st=copyledger(ledger,&candidate);
    if(st!=OK)
    {
        return st;
    }
    st=applycommand(&candidate,command);
    if(st!=OK)
    {
        freeledger(&candidate);
        return st;
    }
    FILE *f=fopen(s->path,"ab");
    if(!f)
    {
        freeledger(&candidate);
        return IO_ERROR;
    }
    unsigned long seq=s->sequence+1;
    int bad=fprintf(f,"%lu %08x %s\nCOMMIT %lu\n",seq,(unsigned)checksum(command),command,seq)<0;
    if(!durable_close(f))
    {
        bad=1;
    }
    if(bad)
    {
        s->poisoned=1;
        freeledger(&candidate);
        return IO_ERROR;
    }
    freeledger(ledger);
    *ledger=candidate;
    s->sequence=seq;
    return OK;
}

Status storage_checkpoint(Storage *s)
{
    if(s->poisoned)
    {
        return IO_ERROR;
    }
    char path[1100];
    snprintf(path,sizeof(path),"%s.tmp",s->path);
    FILE *in=fopen(s->path,"rb");
    if(!in)
    {
        return IO_ERROR;
    }
    FILE *out=fopen(path,"wb");
    if(!out)
    {
        fclose(in);
        return IO_ERROR;
    }
    char buf[4096];
    size_t n;
    int bad=0;
    while((n = fread(buf, 1, sizeof(buf), in)) != 0)
    {
        if(fwrite(buf, 1, n, out) != n)
        {
            bad = 1;
            break;
        }
    }
    if(ferror(in))
    {
        bad=1;
    }
    if(fclose(in)!=0)
    {
        bad=1;
    }
    if(!durable_close(out))
    {
        bad=1;
    }
    if(bad)
    {
        return IO_ERROR;
    }
#ifdef _WIN32
    return MoveFileExA(path,s->path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?OK:IO_ERROR;
#else
    return rename(path,s->path)==0?OK:IO_ERROR;
#endif
}
