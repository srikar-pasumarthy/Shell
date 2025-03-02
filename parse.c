/* Tar Heel SHell
 *
 * This module implements command parsing, following the grammar
 * in the assignment handout.
 */

#include <sys/types.h>
#include "thsh.h"
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

/* This function returns one line from input_fd
 *
 * buf is populated with the contents, including the newline, and
 *      including a null terminator.  Must point to a buffer
 *      allocated by the caller, and may not be NULL.
 *
 * size is the size of *buf
 *
 * Return value: the length of the string (not counting the null terminator)
 *               zero indicates the end of the input file.
 *               a negative value indicates an error (e.g., -errno)
 */
int read_one_line(int input_fd, char *buf, size_t size) {
    int count, rv;
    // pointer to next place in cmd to store a character
    char *cursor;
    // the last character that was written into cmd
    char last_char;

    assert (buf);

    /*
     * We want to continue reading characters until:
     *   - read() fails (rv will become 0) OR
     *   - count == MAX_INPUT-1 (we have no buffer space left) OR
     *   - last_char was '\n'
     * so we continue the loop while:
     *   rv is nonzero AND count < MAX_INPUT - 1 AND last_char != '\n'
     *
     * On every iteration, we:
     *   - increment cursor to advance to the next char in the cmd buffer
     *   - increment count to indicate we've stored another char in cmd
     *     (this is done during the loop termination check with "++count")
     *
     * To make the termination check work as intended, the loop starts by:
     *   - setting rv = 1 (so it's initially nonzero)
     *   - setting count = 0 (we've read no characters yet)
     *   - setting cursor = cmd (cursor at start of cmd buffer)
     *   - setting last_char = 1 (so it's initially not '\n')
     *
     * In summary:
     *   - START:
     *      set rv = 1, count = 0, cursor = cmd, last_char = 1
     *   - CONTINUE WHILE:
     *      rv (is nonzero) && count < MAX_INPUT - 1 && last_char != '\n'
     *   - UPDATE PER ITERATION:
     *      increment count and cursor
     */
    for (rv = 1, count = 0, cursor = buf, last_char = 1;
            rv && (last_char != '\n') && (++count < (size - 1)); cursor++) {

        // read one character
        // file descriptor 0 -> reading from stdin
        // writing this one character to cursor (current place in cmd buffer)
        rv = read(input_fd, cursor, 1);
        last_char = *cursor;
    }
    // null terminate cmd buffer (so that it will print correctly)
    *cursor = '\0';

    // Deal with an error from the read call
    if (!rv) {
        count = -errno;
    }

    return count;
}

/* Check is a file matches a glob.
 *
 * This function takes in a simple file glob (such as '*.c')
 * and a file name, and returns 1 if it matches, and 0 if not.
 */
static int glob_matches(const char *glob, char *name) {
    return strstr(name, glob) != NULL && name[0] != '.';
}

/* Expand a file glob.
 *
 * This function takes in a simple file glob (such as '*.c')
 * and expands it to all matching files in the current working directory.
 * If the glob does not match anything, pass the glob through to the application as an argument.
 *
 * glob: The glob must be simple - only one asterisk as the first character,
 * with 0+ non-special characters following it.
 *
 * buf: A double pointer to scratch buffer allocated by the caller and good for the life of the command,
 *      which one may place expanded globs in.  The double pointer allows you to update the offset
 *      into the buffer.
 *
 * bufsize: _pointer_ to the remaining space in the buffer.  If too many files match, this space may be exceeded,
 *      in which case, the function returns -ENOSPC
 *
 * commands: Same argumetn as passed to parse_line
 *
 * pipeline_idx: Index into command array for the current pipeline
 *
 * arg_idx: _Pointer_ to the current argument index.  May be incremented as
 *         a glob is expanded.
 *
 * Returns 0 on success, -errno on error
 */
static int expand_glob(char *glob, char **buf, size_t *bufsize,
        char *commands [MAX_PIPELINE][MAX_ARGS], int pipeline_idx, int *arg_idx) {

    DIR *d = opendir(".");
    struct dirent *curr = readdir(d);
    
    int found = 0;

    glob++;
    while (curr != NULL) {
        if (glob_matches(glob, curr->d_name)) {
          commands[pipeline_idx][(*arg_idx)++] = curr->d_name;
          found++;
        }
        curr = readdir(d);
    }

    closedir(d);
    return found;
}


/* Parse one line of input.
 *
 * This function should populate a two-dimensional array of commands
 * and tokens.  The array itself should be pre-allocated by the
 * caller.
 *
 * The first level of the array is each stage in a pipeline, at most MAX_PIPELINE long.
 * The second level of the array is each argument to a given command, at most MAX_ARGS entries.
 * In each command buffer, the entry after the last valid entry should be NULL.
 * After the last valid pipeline buffer, there should be one command entry with just a NULL.
 *
 * inbuf: a NULL-terminated buffer of input.
 *        This buffer may be changed by the function
 *        (e.g., changing some characters to \0).
 *
 * length: the length of the string in inbuf.  Should be
 *         less than the size of inbuf.
 *
 * commands: a two-dimensional array of character pointers, allocated by the caller, which
 *           this function populates.
 *
 * scratch: A caller-allocated buffer that can be used for scratch space, such as
 *          expanding globs in the challenge problems.  You may not need to use this
 *          for the core assignment.
 *
 * scratch_len: Size of the scratch buffer
 *
 * return value: Number of entries populated in commands (1+, not counting the NULL),
*               or -errno on failure.
*
*               In the case of a line with no actual commands (e.g.,
        *               a line with just comments), return 0.
*/
int parse_line (char *inbuf, size_t length,
        char *commands [MAX_PIPELINE][MAX_ARGS],
        char **infile, char **outfile,
        char *scratch, size_t scratch_len) {

    //When we see the start of a comment we replace # with \0 to end parsing
    for (int index = 0; inbuf[index]; index++) {
        if (inbuf[index] == '#') {
            inbuf[index] = '\0';
            break;
        }
    }

    //Set up splitting
    char* buffer_ptr = NULL;
    char* buffer_token = strtok_r(inbuf, "|", &buffer_ptr);
    int i = 0;
    int j = 0;

    //Loop while we have a token left in the input
    while (buffer_token) {
        j = 0;
        char* token_ptr = NULL;
        char* command = NULL;

        // Check if the token is handling infile or outfile case
        if(strstr(buffer_token, "<") || strstr(buffer_token, ">")) {
            int in = strstr(buffer_token, "<") ? 1 : 0;

            command = strtok_r(buffer_token, "<>", &token_ptr);
            if (in) {
                *infile = strtok_r(NULL, "< \n", &token_ptr);
            } else {
                *outfile = strtok_r(NULL, "> \n", &token_ptr);
            }
        }
        //Split original token into individual commands/args e.g. "ls -a" splits to "ls" and "-a" 
        char* command_ptr = NULL;
        char* command_token = strtok_r(command ? command : buffer_token, " \n", &command_ptr);
        if (command_token) {
            //Write to output while we have tokens
            while (command_token) {
                if (strstr(command_token, "*.") != NULL) {
                    // WE ARE GLOBBING
                    int rv = expand_glob(command_token, NULL, NULL, commands, i, &j);
                    if (rv == 0) {
                        commands[i][j++] = command_token;
                    }    
                } else {
                    commands[i][j++] = command_token;
                }

                command_token = strtok_r(NULL, " \n", &command_ptr);
            }

            //Get next token from input buffer
            i++;
        }

        buffer_token = strtok_r(NULL, "|", &buffer_ptr);
    }

    return i;
}

