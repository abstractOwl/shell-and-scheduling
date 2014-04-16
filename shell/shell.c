#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
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
#define LINE_MAX 1024
#define PROMPT   "sish:> "


// Execute a file
// TODO: You don't actually need `file`, it should be args[0] anyways
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

  if (pid == -1) {
    // Fork error
    perror("ERROR");
  } else if (pid == 0) {
    // If child, execute process

    // Set IO redirections
    if (in_stream != STDIN_FILENO) {
      dup2(in_stream, STDIN_FILENO);
    }
    if (out_stream != STDOUT_FILENO) {
      dup2(out_stream, STDOUT_FILENO);
    }

    // Execute file
    execvp(file, args);
    perror("ERROR");

    close(in_stream);
    close(out_stream);
  } else if (bg == 0) {
    // Wait for child to finish executing if not bg task
    int status;
    waitpid(pid, &status, 0);
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

// Remove character from end of line, if exists
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

// Pass in char[][] array so it doesn't get scoped out
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
    tokens[index++] = token;
  }
  tokens[index++] = NULL;
  free(line_copy);
}

// Returns the exit status, e.g. 0 on normal exit
void run(void)
{
  while (1) {
    char line[LINE_MAX + 1]; // Account for 150 characters + null-terminator
    printf("%s", PROMPT);

    if (fgets(line, sizeof(line), stdin) == NULL
        || strcmp(line, "exit\n") == 0) {
      return;
    } else {
      chomp(line, '\n');
      LOG("You entered: '%s'\n", line);

      // Check for trailing '&' (backgrounding)
      int is_bg = 0;
      if (line[strlen(line) - 1] == '&') {
        chomp(line, '&');
        chomp(line, ' '); // chomp in case of space before '&'
        is_bg = 1;
      }

      // Split line based on pipes
      int  num_pipes = token_count(line, "|");
      char *commands[num_pipes];
      token_split(line, "|", commands);

      int i;
      for (i = 0; i < num_pipes; i++) {
        int in_stream  = STDIN_FILENO;
        int out_stream = STDOUT_FILENO;

        if (i == 0) {
          // First element may have input redirection
          int num_token = token_count(commands[0], "<");
          if (num_token - 1 == 2) {
            char *parts[num_token];
            token_split(commands[0], "<", parts);

            in_stream = open(
               parts[1],
               O_CREAT | O_TRUNC | O_WRONLY,
               S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR
             );
          } else if (num_token - 1 > 2) {
            fprintf(stderr, "ERROR: Multiple input redirections are not allowed.");
          }
        } else if (i == num_pipes - 1) {
          // Last element may have output redirection
          int num_token = token_count(commands[num_pipes - 1], ">");
          if (num_token - 1 == 2) {
            char *parts[num_token];
            token_split(commands[num_pipes - 1], ">", parts);

            out_stream = open(
               parts[1],
               O_CREAT | O_TRUNC | O_WRONLY,
               S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR
             );
          } else if (num_token - 1 > 2) {
            fprintf(stderr, "ERROR: Multiple output redirections are not allowed.");
          }
        }

        // Run command
        printf("%s, %d, %d\n", commands[i], in_stream, out_stream);

        if (in_stream != STDIN_FILENO) {
          close(in_stream);
        } else if (out_stream != STDOUT_FILENO) {
          close(out_stream);
        }
      }

      //int pipefd[2];
      //if (pipe(pipefd) == -1) {
      //  perror("ERROR");
      //}

      // code for single command
      char *args[token_count(line, " ")];
      token_split(line, " ", args);
      execute(args[0], args, STDIN_FILENO, STDOUT_FILENO, is_bg);
    }
  }
}

// shell.c entrypoint
int main(int argc, char **argv)
{
  run();
  return 0;
}

