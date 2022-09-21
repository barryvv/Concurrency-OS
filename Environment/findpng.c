#include "findpng.h"
#include <stdlib.h>
#include <stdio.h>
#include "lab_png.h"

int is_png_ez(char* buf){
  
  char character[] = ".png";
  const char* cut_buf=&buf[strlen(buf)-4];
  char *ptr = strstr(cut_buf,character);
  if(ptr){
    return 1;
  }
  else {
    return 0;
  }
}

int readFile(char *basePath,int *count){
    DIR *dir;
    struct dirent *ptr;
    char base[1000];

    if((dir=opendir(basePath))==NULL){
        perror("Open dir error");
        exit(1);
    }
    while((ptr=readdir(dir))!= NULL){
        char* str_path = ptr->d_name;  /* relative path name! */
        if(strcmp(str_path,".")==0||strcmp(str_path,"..")==0)
            continue;
        else if(ptr->d_type == 8){  ///file
            memset(base,'\0',sizeof(base));
            strcpy(base,basePath);
            strcat(base,"/");
            strcat(base,str_path);
            if(is_png_ez(str_path)==1){
                if(is_png_find(base)==0){
                printf("%s\n", base);
                *count=1;
                }
            }
        }
        else if(ptr->d_type == 4){ ///dir
              memset(base,'\0',sizeof(base));
              strcpy(base,basePath);
              strcat(base,"/");
              strcat(base,str_path);
              readFile(base,count);
        }
    }
    closedir(dir);
    return 1;
}

int is_png_find(char* filename){
    FILE* fp=fopen(filename,"rb");
    U8 buf[8]={0};
    int c;
    for(int i=0;i<8;i++){
      c=fgetc(fp);
      buf[i]=c;
    }
    U8 comparr[8] = { 137,80,78,71,13,10,26,10 };
	  int flag1= 1;
	for (int i = 0; i < 8; i++) {
    // printf("\n%d " ,buf[i]);
    // printf("%d ",comparr[i]);
		if (buf[i] != comparr[i]) {
			flag1=0;
			break;
		}
	}
    if(flag1==0){
      return 1;
  }
  // printf("\n");
  return 0;
}


int main(){
  int count=0;
  char basePath[1000];
  memset(basePath,'\0',sizeof(basePath));
  strcpy(basePath,".");
  readFile(basePath,&count);
      if(count==0){
      printf("%s","findpng: No PNG file found");
    }
  return 0;
}
