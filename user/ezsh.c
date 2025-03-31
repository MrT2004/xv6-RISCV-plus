#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define LINE_BUFSIZE 1024
#define ARG_BUFSIZE 64
#define NULL 0

char *builtin_str[] = {

};
int (*builtin_func[])(char **) = {

};
int num_builtins()
{
  return sizeof(builtin_str) / sizeof(char *);
}

void add_history(char *line)
{
  // Add the line to history
}
void *reallocateMemory(void *ptr, uint old_size, uint new_size)
{
  if (new_size <= old_size)
    return ptr;

  void *new_ptr = malloc(new_size);
  if (new_ptr == NULL)
    return NULL;

  if (ptr != NULL)
  {
    memmove(new_ptr, ptr, old_size);
    free(ptr);
  }

  return new_ptr;
}
char *find(const char *s, int c)
{
  while (*s != (char)c)
  {
    if (*s == '\0')
    {
      return NULL;
    }
    s++;
  }
  return (char *)s;
}
char *tokenize(char *str, const char *delim)
{
  static char *last = NULL;
  char *start;

  if (str == NULL)
  {
    str = last;
  }

  if (str == NULL)
  {
    return NULL;
  }

  // Skip leading delimiters
  while (*str && find(delim, *str))
  {
    str++;
  }

  if (*str == '\0')
  {
    last = NULL;
    return NULL;
  }

  start = str;

  // Find the end of the token
  while (*str && !find(delim, *str))
  {
    str++;
  }

  if (*str)
  {
    *str++ = '\0';
  }

  last = str;
  return start;
}

char *read_line(void)
{
  int bufsize = LINE_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  char c;

  if (!buffer)
  {
    fprintf(2, "ezsh: allocation error\n");
    exit(1);
  }
  while (1)
  {
    if (read(0, &c, 1) == 0 || c == '\n')
    {
      buffer[position] = '\0';
      return buffer;
    }
    else
    {
      buffer[position] = c;
    }
    position++;

    if (position >= bufsize)
    {
      bufsize += LINE_BUFSIZE;
      buffer = reallocateMemory(buffer, bufsize - LINE_BUFSIZE, bufsize);
      if (!buffer)
      {
        fprintf(1, "ezsh: allocation error\n");
        exit(1);
      }
    }
  }
}
char **split_line(char *line)
{
  int bufsize = ARG_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char *));
  char *token;

  if (!tokens)
  {
    fprintf(2, "ezsh: allocation error\n");
    exit(1);
  }

  token = tokenize(line, " \t\r\n\a");
  while (token != NULL)
  {
    tokens[position] = token;
    position++;

    if (position >= bufsize)
    {
      bufsize += ARG_BUFSIZE;
      tokens = reallocateMemory(tokens, bufsize - ARG_BUFSIZE, bufsize);
      if (!tokens)
      {
        fprintf(1, "ezsh: allocation error\n");
        exit(1);
      }
    }
    token = tokenize(NULL, " \t\r\n\a");
  }
  tokens[position] = NULL;
  return tokens;
}
int launch(char **args)
{
  int pid = fork();

  if (pid == 0)
  {
    if (exec(args[0], args) == -1)
    {
      fprintf(2, "ezsh: command not found: %s\n", args[0]);
    }
    exit(1);
  }
  else if (pid < 0)
  {
    fprintf(2, "ezsh: fork failed\n");
  }
  else
  {
    wait(NULL);
  }
  return 1;
}
int execute(char **args)
{
  if (args[0] == NULL)
  {
    return 1;
  }
  for (int i = 0; i < num_builtins(); i++)
  {
    if (strcmp(args[0], builtin_str[i]) == 0)
    {
      return (*builtin_func[i])(args);
    }
  }
  return launch(args);
}

void loop(void)
{
  char *line;
  char **args;
  int status;

  do
  {
    printf("EZ$ ");
    line = read_line();
    if (line == NULL)
    {
      break; // EOF
    }
    add_history(line);
    args = split_line(line);
    status = execute(args);
    free(line);
    free(args);
  } while (status);
}
int main(int argc, char *argv[])
{
  loop();
  exit(0);
}
