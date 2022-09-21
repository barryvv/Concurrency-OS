/**
 * @biref To demonstrate how to use zutil.c and crc.c functions
 *
 * Copyright 2018-2020 Yiqing Huang
 *
 * This software may be freely redistributed under the terms of MIT License
 */

#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */

/******************************************************************************
 * DEFINED MACROS 
 *****************************************************************************/
#define BUF_LEN  (256*16)
#define BUF_LEN2 (256*32)

/******************************************************************************
 * GLOBALS 
 *****************************************************************************/
U8 gp_buf_def[BUF_LEN2]; /* output buffer for mem_def() */
U8 gp_buf_inf[BUF_LEN2]; /* output buffer for mem_inf() */

/******************************************************************************
 * FUNCTION PROTOTYPES 
 *****************************************************************************/

void init_data(U8 *buf, int len);

/******************************************************************************
 * FUNCTIONS 
 *****************************************************************************/

/**
 * @brief initialize memory with 256 chars 0 - 255 cyclically 
 */
void init_data(U8 *buf, int len)
{
    int i;
    for ( i = 0; i < len; i++) {
        buf[i] = i%256;
    }
}

int main (int argc, char **argv)
{
    // printf("A complete list of command line arguments:\n");
    // for (i = 0; i < argc; i++) {
    //     printf("argv[%d]=%s\n", i, argv[i]);
    // }

    FILE *fp=fopen(argv[1],"rb");
        FILE *f=fopen("buf.txt","w+");
    int c;
    int count=0;
    // int reg[8]={137,80,78,71,13,10,26,10};
   while(1){
     c = fgetc(fp);

      if(feof(fp)){

        break;
      }
            count++;

           fprintf(f,"%d ",c);
   }
   fclose(fp);
   fclose(f);
   U8 *def_buf=NULL;
  //  for(int j=0;j<4;j++){
  //  }
  FILE *ff=fopen("buf.txt","rb");
  def_buf=malloc(count);

for(int i=0;i<count;i++){
  int temp=0;
  fscanf(ff,"%d ",&temp);
  def_buf[i]=temp;
}
// for(int i=0;i<count;i++){
//   printf("%d ",def_buf[i]);
// }
  is_png(def_buf,count,argv[1]);
  fclose(ff);
  free(def_buf);
  def_buf=NULL;
    return 0;

}
