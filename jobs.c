/* Tar Heel SHell
 *
 * This file implements functions related to launching jobs and job control.
 */


#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "thsh.h"
#include <string.h>

static char ** path_table;

/* Initialize the table of PATH prefixes.
 *
 * Split the result on the parenteses, and
 * remove any trailing '/' characters.
 * The last entry should be a NULL character.
 *
 * For instance, if one's PATH environment variable is:
 *  /bin:/sbin///
 *
 * Then path_table should be:
 *  path_table[0] = "/bin"
 *  path_table[1] = "/sbin"
 *  path_table[2] = '\0'
 *
 * Returns 0 on success, -errno on failure.
 */
int init_path(void) {
    // Get the path and create a copy
    char* env = getenv("PATH");
    char* copied = malloc((strlen(env) + 1) * sizeof(char));
    strcpy(copied, env);

    // Count the number of ':' in order to determine the size of the path_table
    int count = 0;
    for (size_t i = 0; i < strlen(copied); i++) {
        if (copied[i] == ':') {
            count++;
        }
    }

    // Malloc some space for the path_table
    path_table = malloc((count + 2) * sizeof(char*));

    // Start deliminating the string by ':'
    char* token = strtok(copied, ":");
    int i = 0;

    while (token != NULL) {
        int token_index = strlen(token) - 1;

        // Replace any trailing slashes with end string characters
        while (token_index >= 0 && token[token_index] == '/') {
            token[token_index] = '\0';
            token_index--;
        }

        // Set the path_table to the (modified) token and fetch the next deliminated string
        path_table[i] = token;
        token = strtok(NULL, ":");
        i++;
    }

    return 0;
}

/* Debug helper function that just prints
 * the path table out.
 */
void print_path_table() {
    if (path_table == NULL) {
        printf("XXXXXXX Path Table Not Initialized XXXXX\n");
        return;
    }

    printf("===== Begin Path Table =====\n");
    for (int i = 0; path_table[i]; i++) {
        printf("Prefix %2d: [%s]\n", i, path_table[i]);
    }
    printf("===== End Path Table =====\n");
}


static int job_counter = 0;

struct kiddo {
    int pid;
    struct kiddo *next; // Linked list of sibling processes
};

// A job consists of a unique numeric ID and
// one or more processes
struct job {
    int id;
    struct kiddo *kidlets; // Linked list of child processes
    struct job *next; // Linked list of active jobs
};

// A singly linked list of active jobs.
static struct job *jobbies = NULL;

/* Initialize a job structure
 *
 * Returns an integer ID that represents the job.
 */
int create_job(void) {
    struct job *tmp;
    struct job *j = malloc(sizeof(struct job));
    j->id = ++job_counter;
    j->kidlets = NULL;
    j->next = NULL;
    if (jobbies) {
        for (tmp = jobbies; tmp && tmp->next; tmp = tmp->next) ;
        assert(tmp!=j);
        tmp->next = j;
    } else {
        jobbies = j;
    }
    return j->id;
}

/* Helper function to walk the job list and find
 * a given job.
 *
 * remove: If true, remove this job from the job list.
 *
 * Returns NULL on failure, a job pointer on success.
 */
static struct job *find_job(int job_id, bool remove) {
    struct job *tmp, *last = NULL;
    for (tmp = jobbies; tmp; tmp = tmp->next) {
        if (tmp->id == job_id) {
            if (remove) {
                if (last) {
                    last->next = tmp->next;
                } else {
                    assert (tmp == jobbies);
                    jobbies = NULL;
                }
            }
            return tmp;
        }
        last = tmp;
    }
    return NULL;
}

/* Given the command listed in args,
 * try to execute it and create a job structure.
 *
 * This function does NOT wait on the child to complete,
 * nor does it return an exit code from the child.
 *
 * If the first argument starts with a '.'
 * or a '/', it is an absolute path and can
 * execute as-is.
 *
 * Otherwise, search each prefix in the path_table
 * in order to find the path to the binary.
 *
 * Then fork a child and pass the path and the additional arguments
 * to execve() in the child.  Wait for exeuction to complete
 * before returning.
 *
 * stdin is a file handle to be used for standard in.
 * stdout is a file handle to be used for standard out.
 *
 * If stdin and stdout are not 0 and 1, respectively, they will be
 * closed in the parent process before this function returns.
 *
 * job_id is the job_id allocated in create_job
 *
 * Returns 0 on success, -errno on failure to create the child.
 *
 */
int run_command(char *args[MAX_ARGS], int stdin, int stdout, int job_id, history *myhistory) {
    // Check if the first arg starts with a '.' or '/'
    if (args[0][0] == '.' || args[0][0] == '/') {
        // Check if the command is here!
        struct stat sb;
        if (stat(args[0], &sb) == 0) {
            // The command is here!
            int pid = fork();
            if (pid == 0) {
                // I am the child
                static char *newenviron[] = { NULL };
                execve(args[0], args, newenviron);
            } else {
                // Je suis the parent
                wait(NULL);
            }
        }

        return 0;
    } 

    int retval = 0;
    int found_builtin = handle_builtin(args, stdin, stdout, &retval, myhistory);

    if (found_builtin == 0) {
        // Loop through all entries in the path table to find the bin file

        for (int i=0; path_table[i]; i++) {
            // Append the cmd to the path_table
            char path[strlen(path_table[i]) + strlen(args[0]) + 2];
            strcpy(path, path_table[i]);
            strcat(path, "/");
            strcat(path, args[0]);

            // Check if the bin file is here.
            struct stat sb;
            if (stat(path, &sb) == 0) {
                // The file exists here!

                // Fork to create child process
                int pid;
                pid = fork();
                if (pid == 0) {
                    // Call execve
                    dup2(stdin, 0);
                    dup2(stdout, 1);
                    static char *newenviron[] = { NULL };
                    execve(path, args, newenviron);
                    return -errno;
                } else {
                    // Wait
                    close(stdout);
                    wait(NULL);
                }
                return 0;
            }
        }
    } else if (found_builtin == -1) {
        // cd failed
        return -1;
    } else {
        return 0;
    }

    (void)&find_job;
    return -2;
}

/* Wait for the job to complete and free internal bookkeeping
 *
 * job_id is the job_id allocated in create_job
 *
 * exit_code is the exit code from the last child process, if it executed.
 *           This parameter may be NULL, and is only set if the return
 *           value is zero.  This is the same as the wstatus parameter
 *           to waitpid variants, and can be used with functions such
 *           as WIFEXITED.  If this job includes multiple
 *           processes, the exit code will be the last process.
 *
 * Returns zero on success, -errno on error.
 */
int wait_on_job(int job_id, int *exit_code) {
    int ret = 0;
    return ret;
}

