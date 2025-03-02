/* COMP 530: Tar Heel SHell */
#include "thsh.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char **argv, char **envp) {
    // flag that the program should end
    bool finished = 0;
    int input_fd = 0; // Default to stdin
    int ret = 0;
    int non_interactive_fd;
    bool non_interactive = 0;
    int debug = 0;
    history *myhistory = malloc(sizeof(struct history));
    myhistory->idx = 0;
    myhistory->valid_entries = 0;
    load_history(myhistory);
    
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        debug = 1;
    } else if (argc > 1) {
        // Open the file and pass the args into stdin
        non_interactive_fd = open(argv[1], O_RDONLY);
        non_interactive = true;
    }

    // Add some error checking code
    ret = init_cwd();
    if (ret) {
        dprintf(2, "Error initializing the current working directory: %d\n", ret);
        return ret;
    }

    ret = init_path();
    if (ret) {
        dprintf(2, "Error initializing the path table: %d\n", ret);
        return ret;
    }

    while (!finished) {
        int length;
        // Buffer to hold input
        char cmd[MAX_INPUT];
        // Buffer for scratch space - optional, only necessary for challenge problems
        char scratch[MAX_INPUT];
        // Get a pointer to cmd that type-checks with char *
        char *buf = &cmd[0];
        char *parsed_commands[MAX_PIPELINE][MAX_ARGS];
        char *infile = NULL;
        char *outfile = NULL;
        int pipeline_steps = 0;

        if (!input_fd) {
            if (!non_interactive) {
                ret = print_prompt();
            } else {
                ret = 1;
            }
            if (ret <= 0) {
                // if we printed 0 bytes, this call failed and the program
                // should end -- this will likely never occur.
                finished = true;
                break;
            }
        }

        // Reset memory from the last iteration
        for(int i = 0; i < MAX_PIPELINE; i++) {
            for(int j = 0; j < MAX_ARGS; j++) {
                parsed_commands[i][j] = NULL;
            }
        }

        // Read a line of input
        if (non_interactive) {
            length = read_one_line(non_interactive_fd, buf, MAX_INPUT);
        } else {
            length = read_one_line(input_fd, buf, MAX_INPUT);
        }

        if (length <= 0) {
            ret = length;
            break;
        }
        // Add it to the history. Yes, I know this is a bit jank but strcmp was acting funny lol
        if (buf[0] != 'e' || buf[1] != 'x' || buf[2] != 'i' || buf[3] != 't') {
            add_history_line(buf, myhistory);
            save_history(myhistory);
        }

        // Pass it to the parser
        pipeline_steps = parse_line(buf, length, parsed_commands, &infile, &outfile, scratch, MAX_INPUT);
        if (pipeline_steps < 0) {
            dprintf(2, "Parsing error.  Cannot execute command. %d\n", -pipeline_steps);
            continue;
        }

        ret = 0;
        // Check if there is a command to run.
        if (pipeline_steps > 0) {
            // Set up the pipes for the program
            int pipes[2 * (pipeline_steps - 1)];
            for (int i = 0; i < 2 * (pipeline_steps - 1); i += 2) {
                pipe(pipes + i);
            }

            // Dup the stdin and stdout pipes for use later
            int og_in = dup(0);
            int og_out = dup(1);

            // Loop through each instruction
            for (int i=0; i< pipeline_steps; i++) {
                if (debug) {
                    // Print debugging statements if necessary
                    fprintf(stderr, "RUNNING: [%s]\n", parsed_commands[i][0]);
                }

                // If we are the first instruction then:
                // 1. Open a file to read/write from if neccessary
                // 2. Check if there is an instruction that requires the result from this instruction
                // 3. Close the file 
                if (i == 0) {
                    if (infile != NULL) {
                        // We have a in file to read from
                        int fileh = open(infile, O_RDONLY);
                        ret = run_command(parsed_commands[i], fileh, pipeline_steps == 1 ? 1 : pipes[1], 0, myhistory);
                        close(fileh);
                    } else if (pipeline_steps == 1 && outfile != NULL) {
                        // We have an out file to write to. Create if necessary
                        int fileh = open(outfile, O_WRONLY | O_CREAT | O_TRUNC); 
                        ret = run_command(parsed_commands[i], 0, fileh, 0, myhistory);
                        close(fileh); 
                    } else {
                        // We are not doing anything with input/output files
                        ret = run_command(parsed_commands[i], 0, pipeline_steps == 1 ? 1 : pipes[1], 0, myhistory);
                    }
                }
                // If we are the last instruction then:
                // 1. Open a file to write to if necessary
                // 2. Write to the last pipe
                // 3. Close the file 
                else if (i == pipeline_steps - 1) {
                    close(pipes[((i - 1) * 2) + 1]);
                    if (outfile != NULL) {
                        // We have an outfile that we need to write to
                        int fileh = open(outfile, O_WRONLY | O_CREAT | O_TRUNC); 
                        ret = run_command(parsed_commands[i], pipes[(i - 1) * 2], fileh, 0, myhistory);
                        close(fileh);
                    } else {
                        // We don't have an outfile to write to so we need to send this to stdout
                        ret = run_command(parsed_commands[i], pipes[(i - 1) * 2], 1, 0, myhistory);
                    }
                } 
                // If we are doing some middle instruction then:
                // 1. Just straight up connect pipes. 
                else {
                    ret = run_command(parsed_commands[i], pipes[(i - 1) * 2], pipes[(i * 2) + 1], 0, myhistory);
                }

                if (debug) {
                    fprintf(stderr, "ENDED: [%s] (ret=%d)\n", parsed_commands[i][0], ret);
                }
            }

            // Reset stdin and stdout
            dup2(og_in, 0);
            close(og_in);

            dup2(og_out, 1);
            close(og_out);

        }
        //    int jobid = create_job();

        if (ret) {
            char buf [100];
            int rv = snprintf(buf, 100, "Failed to run command - error %d\n", ret);
            if (rv > 0) {
                // Write to stdout as usual
                write(1, buf, strlen(buf));
            }
            else {
                dprintf(2, "Failed to format the output (%d).  This shouldn't happen...\n", rv);
            }
        }

    }


    // Only return a non-zero value from main() if the shell itself
    // has a bug.  Do not use this to indicate a failed command.
    return 0;
}
