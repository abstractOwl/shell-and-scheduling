#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LINE_MAX 1024
#define PROMPT   "sish:> "


// Execute a file
// TODO: You don't actually need `file`, it should be args[0] anyways
void execute(const char *file, char **args, int in_stream, int out_stream)
{
  pid_t pid = fork();

  if (pid == 0) {
    // If child, execute process
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
void execute_ls(void)
{
  char *args[2];
  args[0] = "ls";
  args[1] = NULL; // args array *MUST* be terminated by a NULL ptr

  int out_stream = open(
      "test.txt",
      O_CREAT | O_TRUNC | O_WRONLY,
      S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR
    );

  execute(args[0], args, STDIN_FILENO, out_stream);
}

// Remove trailing newline, if any
void chomp(const char line[])
{
  char *nl_index = strchr(line, '\n');
  if (nl_index != NULL) {
    *nl_index = '\0';
  }
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
      printf("You entered: %s\n", line);
      execute_ls();
    }
  }
}

// shell.c entrypoint
int main(int argc, char **argv)
{
  run();
  return 0;
}

