#define _POSIX_C_SOURCE 200809L
#include "canteen_menu.h"
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static void displaymenu(void)
{
    printf("+----------------------------------------------------------+\n");
    printf("|                  CanteenLedger - MENU                    |\n");
    printf("+----------------------------------------------------------+\n");
    printf("| student  | Add a student                                 |\n");
    printf("| topup    | Add money to a wallet                         |\n");
    printf("| item     | Add or change a menu item                     |\n");
    printf("| stock    | Set stock for an item and date                |\n");
    printf("| order    | Place an immediate order or preorder          |\n");
    printf("| cancel   | Cancel a preorder and refund the wallet        |\n");
    printf("| fulfil   | Fulfil a confirmed preorder                   |\n");
    printf("| report   | Display students, menu, stock and orders       |\n");
    printf("| history  | Display saved transactions                    |\n");
    printf("| save     | Save a replacement copy of the log            |\n");
    printf("| exit     | Exit the program                              |\n");
    printf("+----------------------------------------------------------+\n");
    printf("student ID BALANCE NAME (use underscores for spaces)\n");
    printf("topup STUDENT AMOUNT\n");
    printf("item ID PRICE ACTIVE(0/1) NAME\n");
    printf("stock ITEM YYYY-MM-DD QUANTITY\n");
    printf("order STUDENT ITEM QUANTITY TODAY TARGET\n");
    printf("cancel ORDER TODAY | fulfil ORDER TODAY\n");
    printf("report STUDENT(0=all) ITEM(0=all) DATE(*=all) ");
    printf("STATUS(*|0=confirmed|1=fulfilled|2=cancelled)\n");
    printf("Type help to display this menu again.\n");
}

void displayhistory(const Storage *storage)
{
    FILE *fp = fopen(storage->path, "rb");
    if(fp == NULL)
    {
        printf("I/O error\n");
        return;
    }

    char line[1024];
    while(fgets(line, sizeof(line), fp) != NULL)
    {
        if(fputs(line, stdout) == EOF)
        {
            break;
        }
    }
    if(ferror(fp))
    {
        printf("I/O error\n");
    }
    if(fclose(fp) != 0)
    {
        printf("I/O error\n");
    }
}

static void displayreport(const Ledger *ledger, const char *arguments)
{
    char studenttext[32], itemtext[32], date[32], statustext[32], extra;
    int studentid, itemid, status = -1;
    int fields = sscanf(arguments, "%31s %31s %31s %31s %c",
                        studenttext, itemtext, date, statustext, &extra);
    if(fields != 4 || !number(studenttext, &studentid) ||
       !number(itemtext, &itemid))
    {
        printf("invalid filter\n");
        return;
    }
    if(strcmp(date, "*") != 0 && !validdate(date))
    {
        printf("invalid filter\n");
        return;
    }
    if(strcmp(statustext, "*") != 0 &&
       (!number(statustext, &status) || status > 2))
    {
        printf("invalid filter\n");
        return;
    }
    report(ledger, stdout, studentid, itemid, date, status);
}

static void runcli(Ledger *ledger, Storage *storage)
{
    Status result;
    displaymenu();
    char line[LINE_SIZE];
    int input;
    while(fputs("> ", stdout) != EOF && fflush(stdout) == 0 &&
          (input = readline(stdin, line, sizeof(line))) != 0)
    {
        if(input < 0)
        {
            if(ferror(stdin))
            {
                break;
            }
            printf("Invalid input or line too long\n");
            continue;
        }

        char *command = line;
        while(*command == ' ' || *command == '\t')
        {
            command++;
        }
        size_t length = strlen(command);
        while(length > 0 && (command[length - 1] == ' ' ||
                            command[length - 1] == '\t'))
        {
            command[--length] = '\0';
        }
        if(length == 0)
        {
            continue;
        }
        if(strcmp(command, "exit") == 0)
        {
            break;
        }
        if(strcmp(command, "help") == 0)
        {
            displaymenu();
            continue;
        }
        if(strcmp(command, "history") == 0)
        {
            displayhistory(storage);
            continue;
        }
        if(strncmp(command, "report", 6) == 0 &&
           (command[6] == ' ' || command[6] == '\t' || command[6] == '\0'))
        {
            displayreport(ledger, command + 6);
            continue;
        }

        if(strcmp(command, "save") == 0)
        {
            result = storage_checkpoint(storage);
        }
        else
        {
            result = storage_commit(storage, ledger, command);
        }
        printf("%s\n", statusmessage(result));
        if(storage->poisoned)
        {
            break;
        }
    }
}

int main(int argc, char **argv)
{
    const char *directory = ".";
    int mode = 0;
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--menu") == 0 || strcmp(argv[i], "--cli") == 0)
        {
            int selected = strcmp(argv[i], "--menu") == 0 ? 1 : 2;
            if(mode != 0 && mode != selected)
            {
                fprintf(stderr, "Use either --menu or --cli, not both.\n");
                return 2;
            }
            mode = selected;
        }
        else if(strcmp(argv[i], "--data") == 0)
        {
            if(i + 1 == argc || strncmp(argv[i + 1], "--", 2) == 0 || !argv[i + 1][0])
            {
                fprintf(stderr, "--data needs an existing directory.\n");
                return 2;
            }
            directory = argv[++i];
        }
        else if(strcmp(argv[i], "--help") != 0)
        {
            fprintf(stderr, "Unknown argument: %s. Use --help for usage.\n", argv[i]);
            return 2;
        }
    }
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--help") == 0)
        {
            printf("Usage: %s [--menu | --cli] [--data EXISTING_DIRECTORY]\n"
                   "Terminal input opens the menu; piped input uses commands.\n", argv[0]);
            return 0;
        }
    }
    if(mode == 0)
    {
#ifdef _WIN32
        mode = _isatty(_fileno(stdin)) ? 1 : 2;
#else
        mode = isatty(STDIN_FILENO) ? 1 : 2;
#endif
    }
    Ledger ledger = {0};
    Storage storage;
    Status result = storage_open(&storage, &ledger, directory);
    if(result != OK)
    {
        fprintf(stderr, "Load: %s\n", statusmessage(result));
        freeledger(&ledger);
        return 1;
    }
    if(mode == 1)
    {
        runmenu(&ledger, &storage);
    }
    else
    {
        runcli(&ledger, &storage);
    }

    int failed = ferror(stdin) || ferror(stdout) || storage.poisoned;
    if(storage.poisoned)
    {
        fprintf(stderr, "Could not confirm the save. Restart to recover.\n");
    }
    if(fflush(stdout) != 0)
    {
        failed = 1;
    }
    freeledger(&ledger);
    return failed ? 1 : 0;
}
