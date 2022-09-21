#include "lab_png.h"
#include "crc.h"
#include <stdlib.h>
#include <stdio.h>

int is_png(U8* buf, size_t n,char* argv){
  U8 *idat=NULL;
  U8 *ihdr=NULL;
  U8 *iend=NULL;
  struct data_IHDR *di=NULL;
  di=malloc(sizeof(struct data_IHDR));
  int idat_len=n-41-12;
  int ihdr_len=17;
  int iend_len=4;
  idat=malloc(idat_len);
  ihdr=malloc(ihdr_len);
  iend=malloc(iend_len);
    U8 comparr[8] = { 137,80,78,71,13,10,26,10 };
	  int flag1= 1;
    int flag2=1;
    int flag3=1;
    int flag4=1;
	for (int i = 0; i < 8; i++) {
    // printf("\n%d " ,buf[i]);
    // printf("%d ",comparr[i]);
		if (buf[i] != comparr[i]) {
			flag1=0;
			break;
		}
	}
    if(flag1==0){
    printf("%s: Not a PNG file\n",argv);
      free(di);
  di=NULL;
  free(idat);
  idat=NULL;
  free(ihdr);
  ihdr=NULL;
  free(iend);
  iend=NULL;
    return 0;
  }
  // printf("\n");
  int count=0;


        // printf("ihdr: ");
  for(int i=12;i<29;i++){
    ihdr[count]=buf[i];
        // printf("%d ",ihdr[count]);
    count++;
  }
    U32 width=0;        /* width in pixels, big endian   */
    U32 height=0;       /* height in pixels, big endian  */

    width=(ihdr[4]<<24)+(ihdr[5]<<16)+(ihdr[6]<<8)+(ihdr[7]);
    height=(ihdr[8]<<24)+(ihdr[9]<<16)+(ihdr[10]<<8)+(ihdr[11]);
    printf("%s: %d x %d\n",argv,width,height);
  U8 ihdr_crc_comparr[4]={buf[29],buf[30],buf[31],buf[32]};
  unsigned long get_ihdr_crc=crc(ihdr,ihdr_len);
  // printf("\n%lx\n",get_ihdr_crc);
  U8 ihdr_crc_val[4]={0};
  ihdr_crc_val[0]=get_ihdr_crc>>24;
  // printf("crc 1: %02x ",ihdr_crc_comparr[0]);
  ihdr_crc_val[1]=(get_ihdr_crc<<8)>>24;
    // printf("crc 2: %02x ",ihdr_crc_comparr[1]);
  ihdr_crc_val[2]=(get_ihdr_crc<<16)>>24;
    // printf("crc 3: %02x ",ihdr_crc_comparr[2]);
  ihdr_crc_val[3]=(get_ihdr_crc<<24)>>24;
    // printf("crc 4: %02x \n",ihdr_crc_comparr[3]);
  	for (int i = 0; i < 4; i++) {
		if (ihdr_crc_val[i] != ihdr_crc_comparr[i]) {
			flag2=0;
			break;
		}
	}
  if(flag2==0){
        printf("IHDR chunk CRC error: computed %lx, expected %02x%02x%02x%02x",get_ihdr_crc,ihdr_crc_comparr[0],ihdr_crc_comparr[1],ihdr_crc_comparr[2],ihdr_crc_comparr[3]);
          free(di);
  di=NULL;
  free(idat);
  idat=NULL;
  free(ihdr);
  ihdr=NULL;
  free(iend);
  iend=NULL;
    return 0;
  }
    count=0;
        // printf("idat: ");
  for(int j=37;j<(n-16);j++){
    idat[count]=buf[j];
    // printf("%d ",idat[count]);
    count++;
  }
  U8 idat_crc_comparr[4]={buf[n-16],buf[n-15],buf[n-14],buf[n-13]};
  unsigned long get_idat_crc=crc(idat,idat_len);
  // printf("\n%lx\n",get_idat_crc);
  U8 crc_val[4]={0};
  crc_val[0]=get_idat_crc>>24;
  // printf("crc 1: %02x ",idat_crc_comparr[0]);
  crc_val[1]=(get_idat_crc<<8)>>24;
    // printf("crc 2: %02x ",idat_crc_comparr[1]);
  crc_val[2]=(get_idat_crc<<16)>>24;
    // printf("crc 3: %02x ",idat_crc_comparr[2]);
  crc_val[3]=(get_idat_crc<<24)>>24;
    // printf("crc 4: %02x \n",idat_crc_comparr[3]);
  	for (int i = 0; i < 4; i++) {
		if (crc_val[i] != idat_crc_comparr[i]) {
			flag3=0;
			break;
		}
	}
  if(flag3==0){
printf("IDAT chunk CRC error: computed %lx, expected %02x%02x%02x%02x",get_idat_crc,idat_crc_comparr[0],idat_crc_comparr[1],idat_crc_comparr[2],idat_crc_comparr[3]);
  free(di);
  di=NULL;
  free(idat);
  idat=NULL;
  free(ihdr);
  ihdr=NULL;
  free(iend);
  iend=NULL;
return 0;
  }
    count=0;
        // printf("\niend: ");
    for(int i=n-8;i<n-4;i++){
      iend[count]=buf[i];
        // printf("%d ",iend[count]);
    count++;
  }
    U8 iend_crc_comparr[4]={buf[n-4],buf[n-3],buf[n-2],buf[n-1]};
  unsigned long get_iend_crc=crc(iend,iend_len);
  // printf("\n%lx\n",get_iend_crc);
  U8 iend_crc_val[4]={0};
  iend_crc_val[0]=get_iend_crc>>24;
  // printf("crc 1: %02x ",iend_crc_comparr[0]);
  iend_crc_val[1]=(get_iend_crc<<8)>>24;
    // printf("crc 2: %02x ",iend_crc_comparr[1]);
  iend_crc_val[2]=(get_iend_crc<<16)>>24;
    // printf("crc 3: %02x ",iend_crc_comparr[2]);
  iend_crc_val[3]=(get_iend_crc<<24)>>24;
    // printf("crc 4: %02x \n",iend_crc_comparr[3]);
  	for (int i = 0; i < 4; i++) {
		if (iend_crc_val[i] != iend_crc_comparr[i]) {
			flag4=0;
			break;
		}
	}
if(flag4==0){
    printf("IEND chunk CRC error: computed %lx, expected %02x%02x%02x%02x",get_iend_crc,iend_crc_comparr[0],iend_crc_comparr[1],iend_crc_comparr[2],iend_crc_comparr[3]);
      free(di);
  di=NULL;
  free(idat);
  idat=NULL;
  free(ihdr);
  ihdr=NULL;
  free(iend);
  iend=NULL;
    return 0;
  }
  free(di);
  di=NULL;
  free(idat);
  idat=NULL;
  free(ihdr);
  ihdr=NULL;
  free(iend);
  iend=NULL;
  return 0;
}
int get_png_height(struct data_IHDR *buf){

    return 0;
}
int get_png_width(struct data_IHDR *buf){
    return 0;
}
int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence){
    return 0;
}
