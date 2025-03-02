/* Tar Heel SHell
 *
 * This module implements tracking, saving, clearing, and restoring command history.
 *
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "thsh.h"

/* Add a line to the history
*/
void add_history_line(char *line, history *myhistory) {
    // Add the character at idx in arr and increment idx
    strcpy(myhistory->arr[myhistory->idx], line);
    myhistory->idx = (myhistory->idx == 49) ? 0 : myhistory->idx + 1;
    myhistory->valid_entries = (myhistory->valid_entries == 50) ? myhistory->valid_entries : myhistory->valid_entries + 1;
}

int clear_history(char *args[MAX_ARGS], int stdin, int stdout, history *myhistory) {
    myhistory->idx = 0;
    myhistory->valid_entries = 0;

    return 42;
}


int print_history(char *args[MAX_ARGS], int stdin, int stdout, history *myhistory) {
    // Starting at idx, decrement backwards until you valid entries is 0
    int history_len = myhistory->valid_entries;
    int idx = (myhistory->idx - history_len) % 50;

    while (history_len > 0) {
        write(stdout, myhistory->arr[idx], strlen(myhistory->arr[idx]));

        idx++;
        if (idx == 50) idx=0;
        history_len--;
    }

    return 42;
}




int save_history(history *myhistory) {
    // Saves the history in thsh_history.txt
    // Idea: call this function in exit();

    // Starting at idx, decrement backwards until you valid entries is 0
    remove(".history");
    int history_len = myhistory->valid_entries;
    int idx = (myhistory->idx - history_len) % 50;

    // Create a fd to reference thsh_history.txt
    int fileh = open(".history", O_WRONLY | O_CREAT | O_TRUNC | O_APPEND);

    while (history_len > 0) {
        write(fileh, myhistory->arr[idx], strlen(myhistory->arr[idx]));

        idx++;
        if (idx == 50) idx=0;
        history_len--;
    }
    close(fileh);
    return 0;
}

int load_history(history *myhistory) {
    // Loads the history from thsh_history.txt
    FILE *file = fopen(".history", "r");
    char line[MAX_ARGS];
    if (file == NULL) return 0;
    while(fgets(line, sizeof(line), file) != NULL) {
        // Put the line in the appropriate array entry
        char *buf = &line[0];
        add_history_line(buf, myhistory);
    }

    return 0;
}
