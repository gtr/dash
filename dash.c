/*
 *
 * dash: "deadass" shell
 * 
 * Gerardo Torres (github.com/gtr)
 *
 * Instructions for running:
 * =========================================
 * 
 * compile:
 *      $ make
 * run:
 *      $ ./shell
 * clean:
 *      $ make clean
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define TRUE 1          // True
#define FALSE 0         // False
#define MAX_LENGTH 100  // An arbritriary length
#define MAX_TOKENS 50   // An artritriary length

// The user, group, and other permission bits. 
const mode_t CREATE_MODE = S_IRWXU | S_IRWXG | S_IRWXO;

/*
 * IORedirection scans the command for any I/O redirection tokens and uses dup2
 * to redirect input/output.
 *
 * command: the command to scan over
 */
void IORedirection(char** command) {
    int fd, fdFlag;         // File descriptors

    for (int i = 0; command[i] != NULL; i++) {
        char* token = command[i];
        char* file = command[i + 1];
        
        if (strcmp(token, "<") == 0) {
            // Setup read from stdin.
            fd = open(file, O_RDONLY);
            fdFlag = STDIN_FILENO;
        } else if (strcmp(token, ">") == 0) {
            // Setup redirect stdout.
            fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, CREATE_MODE);
            fdFlag = STDOUT_FILENO;
        } else if (strcmp(token, "2>") == 0) {
            // Setup redirect stderr.
            fd = open(file, O_CREAT | O_WRONLY | O_TRUNC , CREATE_MODE);
            fdFlag = STDERR_FILENO;
        } else if (strcmp(token, "&>") == 0) {
            // Setup redirect stderr & stdout (doesnt work).
            fd = open(file, O_CREAT | O_WRONLY | O_APPEND , CREATE_MODE);
            fdFlag = STDERR_FILENO;
        } else if (strcmp(token, ">>") == 0) {
            // Setup append stdout.
            fd = open(file, O_CREAT | O_WRONLY | O_APPEND, CREATE_MODE);
            fdFlag = STDOUT_FILENO;
        } else if (strcmp(token, "2>>") == 0) {
            // Setup append stderr.
            fd = open(file, O_CREAT | O_WRONLY | O_APPEND, CREATE_MODE);
            fdFlag = STDERR_FILENO;
        } else {
            // A redirection token was not matched.
            continue;
        }
        
        // At this point, one of the redirection tokens was used.
        // Check for error when opening.
        if (fd < 0) {
            printf("IORedirection: Error opening file\n");
            exit(1);
        }
        // Check for error when duplicating.
        if (dup2(fd, fdFlag) < 0) {
            printf("IORedirection: Error calling dup\n");
            exit(1);
        }
        // Check for error when closing.
        if (close(fd) < 0) {
            printf("IORedirection: Error closing fd\n");
            exit(1);
        }

        // Remove the redirection token.
        command[i] = NULL;
    }
}

/*
 * execute executes the given argument array, accounting for I/O redirection. 
 * It also makes up the 'print' part of the shell since the child process' 
 * execvp function may produce an output.
 *
 * command: the command to execute
 */
void execute(char** command) {
    // Fork the process into child process to execute the command.
    int pid = fork();
    if (pid == 0) {
        // Check for I/O redirection tokens.
        IORedirection(command);
        if (execvp(command[0], command) < 0) {
            printf("execute: execvp failed\n");
            exit(1);
        }
    } else if (pid < 0) {
        printf("execute: child fork failed\n");
        exit(1);
    } else {
        wait(NULL);
    }
}

/*
 * evaluateCommand is the 'evaluate' part of the REPL sequence for the shell -
 * it evaluates a tokenized command by first checking if the command is a 
 * built-in function and if not, it delegates it to a child execution.
 *
 * command: the command to execute
 * length:  the length of the command array
 */
void evaluateCommand(char** command, int length) {
    // Check if command is exit; if so, exit.
    if (strcmp(command[0], "exit") == 0)
        exit(0);
    
    // Check if command is cd; if so, change directory.
    if (strcmp(command[0], "cd") == 0) {
        char* dir = getenv("HOME");
        if (length > 1)
            dir = command[1];
        if (chdir(dir) != 0) {
            printf("evaluateCommand: cd failed\n");
            exit(1);
        }
        return;
    }

    // If we reached this point, the command is not built-in, delegate to 
    // child process.
    execute(command);
}

/*
 * readCommand is the 'read' part of the REPL sequence for the shell - it reads
 * an input from the user from stdin, tokenizes it, and fills a command array
 *
 * input:   input buffer read from stdin
 * command: command array tokenized from input
 *
 * returns: the length of the command array
 */
int readCommand(char* input, char** command) {
    char* token;        // Token string
    char* delim = " ";  // Delimiter for tokenizing
    int index = 0;      // Starting index to fill command array

    // Read from stdin.
    if (fgets(input, MAX_LENGTH, stdin) == NULL) {
        printf("Error reading from stdin\n");
        exit(1);
    }

    // Make sure input buffer ends with a null character.
    int length = strlen(input);
    if (length == 0) return 0;
    input[length - 1] = '\0';

    // Clear the command array from previous values.
    for (char** ptr = command; *ptr != NULL; ptr++)
        *ptr = NULL;
    
    // Tokenize input into a command array.
    token = strtok(input, delim); 
    command[0] = token;
    while (token != NULL) {
        token = strtok(NULL, delim);
        command[++index] = token;
    }

    return index;
}

/*
 * getPrompt gets the 'PS1' environment prompt or defaults to '-> '.
 */ 
char* getPrompt() {
    char* prompt = getenv("PS1");
	if (!prompt)
		prompt = "-> ";
    return prompt;
}

/*
 * REPL is the function handling the main 'read, evaluate, print, loop' sequence
 * for the shell.
 */
void REPL() {
    char* prompt = getPrompt();
    // The 'loop' part
    while (TRUE) {
        printf("%s", prompt);

        // Set up input and command array for reading.
        char input[MAX_LENGTH];
        char* command[MAX_TOKENS];

        // Read user input and tokenize it into a command.
        int length = readCommand(input, command);
        if (length == 0) {
            // If there is no command, loop again.
            continue;
        }

        // Evaluate the command.
        evaluateCommand(command, length);
    }
}

int main() {
    REPL();     // Start the REPL
}
