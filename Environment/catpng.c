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
#include <netinet/in.h>
/******************************************************************************
 * DEFINED MACROS 
 *****************************************************************************/
#define BUF_LEN  (256*16)
#define BUF_LEN2 (256*128)

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
void catpng(){

}
int main (int argc, char **argv)
{
    // printf("A complete list of command line arguments:\n");
    // for (i = 0; i < argc; i++) {
    //     printf("argv[%d]=%s\n", i, argv[i]);
    // }
    int i;
    data_IDAT_p* idat=NULL;
    idat=malloc((argc-1)*sizeof(struct data_IDAT));
        for(int k = 0; k < argc-1; k++){
            idat[k] = malloc(sizeof *idat);
            
    }
    data_IDAT_p* inf_idat=NULL;
    inf_idat=malloc((argc-1)*sizeof(struct data_IDAT));
        for(int k = 0; k < argc-1; k++){
            inf_idat[k] = malloc(sizeof *inf_idat);
    }
    data_IHDR_p* di = malloc((argc-1)*sizeof(struct data_IHDR));
    for(int k = 0; k < argc-1; k++){
            di[k] = malloc(sizeof *di);
    }
    U32 newH=0;
    U32 newW=0;
    FILE *f1=fopen("text1.txt","w+");
    for (i = 1; i < argc; i++) {
            FILE *fp=fopen(argv[i],"rb");
            fseek(fp,16,SEEK_SET);
            fread(&di[i-1]->width,1,4,fp);
            // fseek(fp, 4, SEEK_CUR);
            fread(&di[i-1]->height,1,4,fp);
            U32 oldH=htonl(di[i-1]->height);
            newW=htonl(di[i-1]->width);
            newH=newH+htonl(di[i-1]->height);
            fseek(fp,9,SEEK_CUR);
            int idatnum=0;
            fread(&idatnum,1,4,fp);
            idatnum=htonl(idatnum);
            fseek(fp,4,SEEK_CUR);
              idat[i-1]->p_data=(U8*)malloc(sizeof(U8)*idatnum);
            for(int j=0;j<idatnum;j++){

              // sp[i-1]->p_IDAT=(chunk_p)malloc(sizeof(chunk_p));
              fread(&idat[i-1]->p_data[j],1,1,fp);

            }
            U64 len_inf=0;
            unsigned long inf_len=oldH*(newW*4+1);
            // U8 inf_idat[inf_len];
            // init_data(inf_idat,inf_len);
            inf_idat[i-1]->p_data=malloc(inf_len);
            // init_data(inf_idat[i-1]->p_data,inf_len);
            int ret = mem_inf(inf_idat[i-1]->p_data, &len_inf, idat[i-1]->p_data, idatnum);
            for(int j=0;j<len_inf;j++){
              fprintf(f1,"%02x ",inf_idat[i-1]->p_data[j]);
            }
            if (ret == 0) { /* success */
            } else { /* failure */
            fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
            }
            inf_idat[i-1]->count=inf_len;
            fclose(fp);
          }

              U64 len_def=0;
              // printf("%d ",newH*(newW*4+1));
              data_IDAT_p allidat=NULL;
              allidat=malloc(sizeof(struct data_IDAT));
              allidat->p_data=malloc(newH*(newW*4+1));
              // allidat->p_data[0] = '\0';
              // for(int i=0;i<90050;i++){
              //   printf("%d ",inf_idat[0]->p_data[i]);
              // }
              // for(int i=0;i<argc-1;i++){
              //   printf("inf count: %d, %d",i,inf_idat[i]->count);
              // // memcpy(allidat->p_data,inf_idat[i]->p_data,inf_idat[i]->count);
              // }
            fclose(f1);
            FILE *f2=fopen("text1.txt","r");
              int c;
              int co=0;
    // int reg[8]={137,80,78,71,13,10,26,10};
              while(1){
              fscanf(f2, "%02x ", &c);
              allidat->p_data[co]=c;
              // printf("%02x ",c);
               if(feof(f2)){
                 break;
                }
               co++;
            }
                int ret = mem_def(gp_buf_def, &len_def,allidat->p_data , newH*(newW*4+1), Z_DEFAULT_COMPRESSION);
    if (ret == 0) { /* success */
    } else { /* failure */
        fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
        return ret;
    }

    FILE *fa=fopen("all.png","wb+");
      U8 set1[16]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82};
      U8 ihdr[17]={0};
      for(int i=0;i<4;i++){
        ihdr[i]=set1[12+i];
      }
      // for(int i=0;i<16;i++){
      //   fprintf(fa,"%x",set1[i]);
      // }
      fwrite(set1,1,16,fa);
      for(int i=0;i<4;i++){
        // fprintf(fa,"%x",(newW<<(i*8))>>24);
        
        ihdr[4+i]=(newW<<(i*8))>>24;
        fwrite(ihdr+4+i,1,1,fa);
      }
      
      for(int i=0;i<4;i++){
        ihdr[8+i]=(newH<<(i*8))>>24;
        
        fwrite(ihdr+8+i,1,1,fa);
      }
      U8 set2[5]={8,6,0,0,0};
      for(int i=0;i<5;i++){
        // fprintf(fa,"%x",set2[i]);
        ihdr[12+i]=set2[i];
        fwrite(ihdr+12+i,1,1,fa);
      }
      U32 ihdr_crc=crc(ihdr,17);
      U8 crc1[4]={0};
      for(int i=0;i<4;i++){
        // fprintf(fa,"%x",(ihdr_crc<<(i*8))>>24);
        crc1[i]=(ihdr_crc<<(i*8))>>24;
      }
      fwrite(crc1,1,4,fa);
      U8 DAT[4]={0};
      for(int i=0;i<4;i++){
        // fprintf(fa,"%lx",(len_def<<(i*8))>>24);
        DAT[i]=(len_def<<(i*8))>>24;
      }
      fwrite(DAT,1,4,fa);
      U8 *p_d=NULL;
      
      p_d=malloc(len_def+4);
      U8 set3[4]={73,68,65,84};
        for(int i=0;i<4;i++){
        // fprintf(fa,"%x",set3[i]);
        p_d[i]=set3[i];
      }
      for(int i=0;i<len_def;i++){
        // fprintf(fa,"%x",gp_buf_def[i]);
        p_d[i+4]=gp_buf_def[i];
      }
      fwrite(p_d,1,len_def+4,fa);
      U32 idat_crc=crc(p_d,len_def+4);
      U8 dcc[4]={0};
      for(int i=0;i<4;i++){
        // fprintf(fa,"%x",(idat_crc<<(i*8))>>24);
        dcc[i]=(idat_crc<<(i*8))>>24;
      }
      fwrite(dcc,1,4,fa);
      U8 set4[12]={0,0,0,0,73,69,78,68,174,66,96,130};
      fwrite(set4,1,12,fa);
      // for(int i=0;i<12;i++){
      //   fprintf(fa,"%x",set4[i]);
      // }
              // for(int i=0;i<newH*(newW*4+1);i++){
              //   // printf("%02x ",allidat->p_data[i]);
              // }
      free(p_d);
      p_d=NULL;
      free(allidat->p_data);
      allidat->p_data=NULL;
      free(allidat);
      allidat=NULL;
      for(int k = 1; k < argc; k++){
        free(idat[k-1]->p_data);
        idat[k-1]->p_data=NULL;
        free(inf_idat[k-1]->p_data);
        inf_idat[k-1]->p_data=NULL;
        free(idat[k-1]);
        idat[k-1]=NULL;
        free(inf_idat[k-1]);
        inf_idat[k-1]=NULL;
        free(di[k]);
        di[k]=NULL;
      }
      free(idat);
      idat=NULL;
      free(inf_idat);
      inf_idat=NULL;
      free(di);
      di=NULL;
      fclose(f2);
      fclose(fa);

    return 0;
//     FILE *fp=fopen(argv[1],"rb");
//         FILE *f=fopen("buf.txt","w+");
//     int c;
//     int count=0;
//     // int reg[8]={137,80,78,71,13,10,26,10};
//    while(1){
//      c = fgetc(fp);

//       if(feof(fp)){

//         break;
//       }
//             count++;

//            fprintf(f,"%d ",c);
//    }
//    fclose(fp);
//    fclose(f);
//    U8 *def_buf=NULL;
//   //  for(int j=0;j<4;j++){
//   //  }
//   FILE *ff=fopen("buf.txt","rb");
//   def_buf=malloc(count);
//   int k=0;

// for(int i=0;i<count;i++){
//   int temp=0;
//   fscanf(ff,"%d ",&temp);
//   def_buf[i]=temp;
// }
// // for(int i=0;i<count;i++){
// //   printf("%d ",def_buf[i]);
// // }
//   int getdaw=is_png(def_buf,count,argv[1]);
//   // fclose(ff);
//     return 0;

}
