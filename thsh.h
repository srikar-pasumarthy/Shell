#ifndef THSH_H
#define THSH_H

/* Do not change this file */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>

// Assume a pipeline will never be longer than 31 stages (+NULL)
#define MAX_PIPELINE   32

// Assume any individual command will not have more than 15 arguments (+NULL)
#define MAX_ARGS       16

// Disallow exec*p* variants, lest we spoil the fun
#pragma GCC poison execlp execvp execvpe

// Data Structure to keep track of history.
typedef struct history {
    int idx;
    int valid_entries;
    char arr[50][MAX_ARGS];
} history;

// Helper functions

// In parse.c:
int read_one_line(int input_fd, char * buf, size_t size);
int parse_line (char *inbuf, size_t length, char *commands [MAX_PIPELINE][MAX_ARGS],
		char **infile, char **outfile,
		char *scratch, size_t scratch_len);

// In builtin.c:
int init_cwd(void);
int handle_builtin(char *args[MAX_ARGS], int stdin, int stdout, int *retval, history *myhistory);
int print_prompt(void);

// In jobs.c:
int init_path(void);
void print_path_table(void);
int create_job(void);
int run_command(char *args[MAX_ARGS], int stdin, int stdout, int job_id, history *myhistory);
int wait_on_job(int job_id, int *exit_code);

// In history.c (optional - challenge only)
void add_history_line(char *line, history *myhistory);
int clear_history(char *args[MAX_ARGS], int stdin, int stdout, history *myhistory);
int print_history(char *args[MAX_ARGS], int stdin, int stdout, history *myhistory);
int save_history(history *myhistory);
int load_history(history *myhistory);

#endif // THSH_H
