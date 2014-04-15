#include <stdio.h>
#include <string.h>

#define LINE_MAX 1024
#define PROMPT   "sish:> "

// Returns the exit status, e.g. 0 on normal exit
void run()
{
  while (1) {
    char line[LINE_MAX + 1]; // Account for null-terminator
    printf("%s", PROMPT);

    if (fgets(line, LINE_MAX + 1, stdin) == NULL
        || strcmp(line, "exit\n") == 0) {
      return;
    } else {
      printf("You entered: %s", line);
    }
  }
}

int main(int argc, char **argv)
{
  run();
  return 0;
}

