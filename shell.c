/**
 * Simple SHell
 * for UCSB CS170
 *
 * Group:
 * - AbstractOwl
 * - Jon Adler
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Print debug messages if DEBUG = 1
#define DEBUG 0
#if DEBUG == 1
#define LOG(...) fprintf(stdout, __VA_ARGS__)
#else
#define LOG(...)
#endif

// Define constants
#define IN_MAX 1024
#define PROMPT "sish:> "


/**
 * Executes a program given arguments and IO params.
 *
 * @param file       Program to run
 * @param args       Argument array, which ends with NULL 0
 * @param in_stream  File descriptor of file to read from
 * @param out_stream File descriptor of file to write to
 * @param bg         Boolean value describing whether to wait for this program
 *                   to finish before continuing
 */
void execute(const char *file, char **args, int in_stream, int out_stream, int bg)
{
#if DEBUG == 1
  LOG("execute - Running `%s`%s with params: [ ", args[0], bg ? "[BG]" : "");
  {
    int i;
    for (i = 1; i < sizeof(args); i++) {
      if (args[i] == NULL) break;
      LOG("'%s' ", args[i]);
    }
  }
  LOG("]\n");
#endif

  pid_t pid = fork();

  // Child must exit
  if (pid == -1) {
    // Fork error
    perror("ERROR");
    exit(EXIT_FAILURE);

  } else if (pid == 0) {
    // If child, execute process

    // Enable interruption signals
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT,  SIG_DFL);

    // Set IO redirections
    dup2(in_stream, STDIN_FILENO);
    dup2(out_stream, STDOUT_FILENO);

    // Execute file
    execvp(file, args);
    perror("ERROR");
    _exit(EXIT_SUCCESS);

  } else if (bg == 0) {
    // Wait for child to finish executing if not bg task
    int status;
    waitpid(pid, &status, 0);

    if (in_stream != STDIN_FILENO) {
      close(in_stream);
    }
    if (out_stream != STDOUT_FILENO) {
      close(out_stream);
    }
  }
}

// Example code for how to use execvp
// void execute_ls(void)
// {
//   char *args[2];
//   args[0] = "ls";
//   args[1] = NULL; // args array *MUST* be terminated by a NULL ptr
// 
//   int out_stream = open(
//       "test.txt",
//       O_CREAT | O_TRUNC | O_WRONLY,
//       S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR
//     );
// 
//   execvp(args[0], args);
// }

/**
 * Removes a character from the end of a line, if it exists. This works by
 * replacing the offending character with a null-terminator '\0'.
 *
 * @param line Pointer to the beginning of the line to operate upon
 * @param junk Character to check for/remove
 */
void chomp(char *line, const char junk)
{
  if (line[strlen(line) - 1] == junk) {
    line[strlen(line) - 1] = '\0';
  }
}

// ----------------------------------------------------------------------------
// token_count & token_split
//
// As a lazy way to avoid realloc messiness, we first use token_count to count
// the number of tokens in the string. We use this integer to create a char[][]
// array big enough to hold all the tokens.
//
// ----------------------------------------------------------------------------

/**
 * Returns the number of tokens in a string that are separated by a specified
 * delimiter.
 *
 * @param line  String to check
 * @param delim Delimiter string
 */
int token_count(const char line[], const char delim[])
{
  // Create copy of line
  int token_count = 0;
  char *line_copy = strdup(line);
  char *token;
 
  while ((token = strsep(&line_copy, delim)) != NULL) {
    ++token_count;
  }
  free(line_copy);
  return token_count + 1; // Add extra space for NULL terminator
}

/**
 * Splits a string around a specified delimiter. Note that the `tokens`
 * array should be a char* array of length calculated by `token_count`.
 *
 * @param line   Line to operate upon
 * @param delim  Delimiter string
 * @param tokens (char*) array to store the tokens in
 */
void token_split(const char line[], const char delim[], char **tokens)
{
  // Create copy of line
  int index = 0;
  char *line_copy = strdup(line);
  char *token;
 
  while ((token = strsep(&line_copy, delim)) != NULL) {
    if (*token == ' ') {
      token++; // Skip leading whitespace
    }
    chomp(token, ' ');
    tokens[index++] = token;
  }
  tokens[index++] = NULL;
  free(line_copy);
}

/**
 * Run loop of this shell.
 */
void run(void)
{
  // Disable SIG_INIT, SIG_TERM
  signal(SIGTERM, SIG_IGN);
  signal(SIGINT,  SIG_IGN);

  while (1) {
    char line[IN_MAX + 1]; // Account for 150 characters + null-terminator
    if (isatty(fileno(stdin))) {
      printf("%s", PROMPT);
    }

    char *result = fgets(line, sizeof(line), stdin);
    LOG("You entered: '%s'\n", line);
    if (result == NULL || strcmp(line, "exit\n") == 0) {
      LOG("Exiting...\n");
      return;
    } else {
      chomp(line, '\n');

      // Check for trailing '&' (backgrounding)
      int is_bg = 0;
      if (line[strlen(line) - 1] == '&') {
        chomp(line, '&');
        chomp(line, ' '); // chomp in case of space before '&'
        is_bg = 1;
      }

      // Split line based on pipes
      // num_pipes accounts for the extra NULL pointer which is not used for
      // pipes array, therefore subtract 2
      int  num_pipes = token_count(line, "|");
      char *commands[num_pipes];
      token_split(line, "|", commands);

      LOG("Found %d pipes\n", num_pipes - 1);

      int i;
      int pipefd[2];  // Passed from previous
      int pipefd2[2]; // Passed to next
      for (i = 0; i < num_pipes - 1; i++) {
        // All commands excluding the first and last *WILL* have 2 pipes,
        // an input from the last command and an output for the next command.
        // We only need to construct one pipe per command, since the other
        // will come from the previous command.
        int in_stream  = STDIN_FILENO;
        int out_stream = STDOUT_FILENO;

        // > is on the outside, should be split off first
        if (i == num_pipes - 2) {
          // Last element may have output redirection
          int num_token = token_count(commands[num_pipes - 2], ">");
          if (num_token - 1 == 2) {
            char *parts[2];
            token_split(commands[num_pipes - 2], ">", parts);

            // Open descriptor for this file
            out_stream = open(
               parts[1],
               O_CREAT | O_TRUNC | O_WRONLY,
               S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR
             );

            // Remove redirection part from original string
            char *pos = strstr(commands[num_pipes - 2], ">");
            *pos = '\0';
            chomp(commands[num_pipes - 2], ' ');
          } else if (num_token - 1 > 2) {
            fprintf(stderr, "ERROR: Multiple output redirections are not allowed.");
          }
        } else {
          if (pipe(pipefd2) == -1) {
            perror("ERROR");
          }
          out_stream = pipefd2[1];
        }

        if (i == 0) {
          // First element may have input redirection
          int num_token = token_count(commands[0], "<");
          if (num_token - 1 == 2) {
            char *parts[2];
            token_split(commands[0], "<", parts);

            in_stream = open(
               parts[1],
               O_RDONLY
             );

            // Remove redirection part from original string
            char *pos = strstr(commands[0], "<");
            *pos = '\0';
            chomp(commands[0], ' ');
          } else if (num_token - 1 > 2) {
            fprintf(stderr, "ERROR: Multiple input redirections are not allowed.");
          }
        } else {
          in_stream = pipefd[0];
        }

        // Don't wait for piped jobs to finish
        if (i != 0 && i != num_pipes - 2) {
          is_bg = 1;
        }

        // Run command
        char *args[token_count(commands[i], " ")];
        token_split(commands[i], " ", args);
        execute(args[0], args, in_stream, out_stream, is_bg);

        pipefd[0] = pipefd2[0];
      }
    }
  }
}

// shell.c entrypoint
int main(int argc, char **argv)
{
  run();
  return 0;
}

