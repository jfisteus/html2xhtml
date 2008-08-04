#include <stdio.h>
#include <stdlib.h>

#include "../charset.h"

/*
 * call:
 * test_charset <from_charset> <input_file>
 *
 */

int main(int argc, char **argv)
{
  char buf[128];
  int read;

  if (argc == 3) {
    FILE *file = fopen(argv[2], "r");
    if (!file) {
      perror("fopen");
      exit(1);
    }
      
    charset_init_input(argv[1], file);
    do {
      read = charset_read(buf, sizeof(buf));
      fwrite(buf, 1, read, stdout);
    } while (read > 0);

    charset_close();

    if (fclose(file)) {
      perror("fclose");
    }
  }
}

int parser_num_linea = -1;
int tree_allocated_memory = -1;

void exit_on_error(char *msg)
{
  fprintf(stderr,"!!%s(%d)[l%d]: %s\n",__FILE__, __LINE__,
	  parser_num_linea, msg);
  exit(1);
}
