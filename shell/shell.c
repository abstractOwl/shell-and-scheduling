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
void execute(const char *file, char **args, int in_stream, int out_stream)
{
#if DEBUG == 1
  LOG("execute - Running '%s' with params: [ ", args[0]);
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

  if (pid == 0) {
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
  } else {
    // Wait for child to finish executing
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
//   execute(args[0], args, STDIN_FILENO, out_stream);
// }

// Remove trailing newline, if any
void chomp(const char line[])
{
  char *nl_index = strchr(line, '\n');
  if (nl_index != NULL) {
    *nl_index = '\0';
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

int token_count(const char line[])
{
  // Create copy of line
  int token_count = 0;
  char *line_copy = strdup(line);
  char *token;
 
  while ((token = strsep(&line_copy, " ")) != NULL) {
    ++token_count;
  }
  free(line_copy);
  return token_count + 1; // Add extra space for NULL terminator
}

// Pass in char[][] array so it doesn't get scoped out
void token_split(const char line[], char **tokens)
{
  // Create copy of line
  int index = 0;
  char *line_copy = strdup(line);
  char *token;
 
  while ((token = strsep(&line_copy, " ")) != NULL) {
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
      chomp(line);
      LOG("You entered: '%s'\n", line);

      char *args[token_count(line)];
      token_split(line, args);
      execute(args[0], args, STDIN_FILENO, STDOUT_FILENO);
    }
  }
}

// shell.c entrypoint
int main(int argc, char **argv)
{
  run();
  return 0;
}

