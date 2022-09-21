#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <curl/curl.h>
#include <getopt.h>
#include <semaphore.h>
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <netinet/in.h>

#define BUF_LEN  (256*16)
#define BUF_LEN2 (480300)

#define IMG_URL "http://ece252-1.uwaterloo.ca:2520/image?img=1"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

/******************************************************************************
 * GLOBALS 
 *****************************************************************************/
U8 gp_buf_def[BUF_LEN2]; /* output buffer for mem_def() */
U8 gp_buf_inf[BUF_LEN2]; /* output buffer for mem_inf() */
sem_t sem[4];
int checkft[50]={0};
int server=3;
pthread_mutex_t mutex[2];
// pthread_cond_t cond[5];
void* do_work(void * param);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
int check(int* checkft);
void round_robin();
void round_robin(){
    if(server==1){
        server=2;
    }else if(server==2){
        server=3;
    }
    else if(server==3){
        server=1;
    }
}

void* do_work(void* param){
    		// pthread_mutex_lock(&mutex);
    while(1){
    sem_wait(&sem[2]);
    pthread_mutex_lock(&mutex[1]);
    if(check(checkft)==1){
        sem_post(&sem[2]);
        pthread_mutex_unlock(&mutex[1]);
        break;
    }else{
        sem_post(&sem[2]);
        pthread_mutex_unlock(&mutex[1]);
    }
    int* img=(int*)param;
    CURL *curl_handle;
    CURLcode res;
    char url[256];
    
    RECV_BUF recv_buf;
    char fname[256];
    // pid_t pid =getpid();
    recv_buf_init(&recv_buf, BUF_SIZE);
    sem_wait(&sem[0]);
    pthread_mutex_lock(&mutex[0]);
    {
    round_robin();
    }
    sem_post(&sem[0]);
    pthread_mutex_unlock(&mutex[0]);
    sem_wait(&sem[0]);
    pthread_mutex_lock(&mutex[0]);
    sprintf(url,"http://ece252-%d.uwaterloo.ca:2520/image?img=%d",server,img[0]);  
    sem_post(&sem[0]);
    pthread_mutex_unlock(&mutex[0]);
    // strcpy(url, argv[1]);
    // printf("URL is %s\n", url);

    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return NULL;
    }
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    
    /* get it! */
    res = curl_easy_perform(curl_handle);
    if( res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        // printf("%d\n",server);
    } 
    sem_wait(&sem[1]);
    pthread_mutex_lock(&mutex[1]);
    
    if(checkft[recv_buf.seq]==0){
        checkft[recv_buf.seq]=1;
        sem_post(&sem[1]);
        pthread_mutex_unlock(&mutex[1]);
        sprintf(fname, "./output_%d.png", recv_buf.seq);
        write_file(fname, recv_buf.buf, recv_buf.size);
    }else{
        sem_post(&sem[1]);
        pthread_mutex_unlock(&mutex[1]);
    }

    /* cleaning up */
    curl_easy_cleanup(curl_handle);
    recv_buf_cleanup(&recv_buf);

    }
    // pthread_mutex_unlock(&mutex);
    // pthread_exit(NULL);
    return NULL;
}
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;
    
    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}
int check(int* checkft){
    for(int i=0;i<50;i++){
        if(checkft[i]==0){
            return 0;
        }
    }
    return 1;
}
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}


int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    
    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	return 2;
    }
    
    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	return 1;
    }
    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3; 
    }
    return fclose(fp);
}

int main(int argc, char **argv) {
      int c;
    int t = 1;
    int n = 1;
    char *str = "option requires an argument";
    
    while ((c = getopt (argc, argv, "t:n:")) != -1) {
        switch (c) {
        case 't':
	    t = strtoul(optarg, NULL, 10);
	    // printf("option -t specifies a value of %d.\n", t);
	    if (t <= 0) {
                fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
            }
            break;
        case 'n':
            n = strtoul(optarg, NULL, 10);
	    // printf("option -n specifies a value of %d.\n", n);
            if (n <= 0 || n > 3) {
                fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
                return -1;
            }
            break;
        default:
            return -1;
        }
    }

    pthread_t *p_tids = malloc(sizeof(pthread_t) * t);
        curl_global_init(CURL_GLOBAL_DEFAULT);
    // char* url[45]=malloc(3*45);

    pthread_mutex_init(&mutex[0],NULL);
    pthread_mutex_init(&mutex[1],NULL);
    // for(int i=0;i<5;i++){
    //         pthread_cond_init(&cond[i],NULL);
    // }
    sem_init(&sem[0],0,1);
    sem_init(&sem[1],0,1);
    sem_init(&sem[2],0,1);
    for(int i=0;i<t;i++){

            int param[1]={n};
            pthread_create(p_tids + i, NULL, do_work,param);
    }
    for(int i=0;i<t;i++){
        pthread_join(p_tids[i],NULL);
    }
        free(p_tids);
        sem_destroy(&sem[0]);
        sem_destroy(&sem[1]);
        sem_destroy(&sem[2]);
        curl_global_cleanup();
        pthread_mutex_destroy(&mutex[0]);
        pthread_mutex_destroy(&mutex[1]);
        /*start catpng*/
    data_IDAT_p* idat=NULL;
    idat=malloc((50)*sizeof(struct data_IDAT));
        for(int k = 0; k < 50; k++){
            idat[k] = malloc(sizeof *idat);
            
    }
    data_IDAT_p* inf_idat=NULL;
    inf_idat=malloc((50)*sizeof(struct data_IDAT));
        for(int k = 0; k < 50; k++){
            inf_idat[k] = malloc(sizeof *inf_idat);
    }
    data_IHDR_p* di = malloc((50)*sizeof(struct data_IHDR));
    for(int k = 0; k < 50; k++){
            di[k] = malloc(sizeof *di);
    }
    U32 newH=0;
    U32 newW=0;
    FILE *f1=fopen("text1.txt","w+");
        for(int i=0;i<50;i++){
        char fname[256];
        sprintf(fname, "./output_%d.png", i);
        // printf("%s\n",fname);
        FILE *fp=fopen(fname,"rb");
        fseek(fp,16,SEEK_SET);
        fread(&di[i]->width,1,4,fp);
        // fseek(fp, 4, SEEK_CUR);
        fread(&di[i]->height,1,4,fp);
        U32 oldH=htonl(di[i]->height);
        newW=htonl(di[i]->width);
        newH=newH+htonl(di[i]->height);
        fseek(fp,9,SEEK_CUR);
        int idatnum=0;
        fread(&idatnum,1,4,fp);
        idatnum=htonl(idatnum);
            fseek(fp,4,SEEK_CUR);
              idat[i]->p_data=(U8*)malloc(sizeof(U8)*idatnum);
            for(int j=0;j<idatnum;j++){

              // sp[i-1]->p_IDAT=(chunk_p)malloc(sizeof(chunk_p));
              fread(&idat[i]->p_data[j],1,1,fp);

            }
            U64 len_inf=0;
            unsigned long inf_len=oldH*(newW*4+1);
            // U8 inf_idat[inf_len];
            // init_data(inf_idat,inf_len);
            inf_idat[i]->p_data=malloc(inf_len);
            // init_data(inf_idat[i-1]->p_data,inf_len);
            int ret = mem_inf(inf_idat[i]->p_data, &len_inf, idat[i]->p_data, idatnum);
            for(int j=0;j<len_inf;j++){
              fprintf(f1,"%02x ",inf_idat[i]->p_data[j]);
            }
            if (ret == 0) { /* success */
            } else { /* failure */
            fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
            }
            inf_idat[i]->count=inf_len;
            fclose(fp);
     }
                   U64 len_def=0;
            //   printf("%d ",newH*(newW*4+1));
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
              int ch;
              int co=0;
    // int reg[8]={137,80,78,71,13,10,26,10};
              while(1){
              fscanf(f2, "%02x ", &ch);
              allidat->p_data[co]=ch;
            //   printf("%02x ",c);
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
}