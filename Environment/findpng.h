#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>   /* for printf().  man 3 printf */
#include <stdlib.h>  /* for exit().    man 3 exit   */
#include <string.h>  /* for strcat().  man strcat   */
#include <stdbool.h>

int is_png_ez(char* buf);
int readFile(char* basePat,int* count);
int is_png_find(char* argv);
int main();
