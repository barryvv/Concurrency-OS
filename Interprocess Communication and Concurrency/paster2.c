#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <semaphore.h>
#include "shm_stack.h"
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"
#define NUM_CHILD 5
#define SEM_PROC 1
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_LEN  (256*16)
#define BUF_LEN2 (480300)
#define BUF_SIZE 10000
typedef struct recv_buf_flat {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

typedef struct int_stack
{
    int size;               /* the max capacity of the stack */
    int pos;                /* position of last item pushed onto the stack */
    RECV_BUF recv_buf[];             /* stack of stored integers */
} ISTACK;

typedef struct ez_buf {
    U8 *buf;       /* memory to hold a copy of received data */
} EZ_BUF;

typedef struct ez_stack
{
    int count;
    EZ_BUF ez[50];             /* stack of stored integers */
} EZTACK;
int producer(int n, struct int_stack *pstack,sem_t *sems,int* img_count);
int consumer(struct int_stack *pstack,sem_t *sems,EZTACK *estack,int* asem_count,int* img_count);
void push_all(struct int_stack *p, int start);
void pop_all(struct int_stack *p);



size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
char * strcpyy( char *dest, const char *src );

/**
 * @brief  cURL header call back function to extract image sequence number from 
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line 
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
char * strcpyy( char *dest, const char *src )
{

    if(dest == src) {   return dest; }

    assert( (dest != NULL) && (src != NULL) );

    char *tmp = dest;
    // printf("src:%s",src);
    while( (*dest++ = * src++) != '\0' ){
        printf("go");
    }

    return tmp;
}
int sizeof_shm_stack(int size)
{
    return (sizeof(ISTACK) + sizeof(char) * size* 10000 + sizeof(RECV_BUF)*size);
}

/**
 * @brief initialize the ISTACK member fields.
 * @param ISTACK *p points to the starting addr. of an ISTACK struct
 * @param int stack_size max. number of items the stack can hold
 * @return 0 on success; non-zero on failure
 * NOTE:
 * The caller first calls sizeof_shm_stack() to allocate enough memory;
 * then calls the init_shm_stack to initialize the struct
 */


/**
 * @brief check if the stack is full
 * @param ISTACK *p the address of the ISTACK data structure
 * @return non-zero if the stack is full; zero otherwise
 */

int is_full(ISTACK *p)
{
    if ( p == NULL ) {
        return 0;
    }
    return ( p->pos == (p->size -1) );
}
int will_full(ISTACK *p){
        if ( p == NULL ) {
        return 0;
    }
    return ( p->pos+1 >= (p->size -1) );
}
/**
 * @brief check if the stack is empty 
 * @param ISTACK *p the address of the ISTACK data structure
 * @return non-zero if the stack is empty; zero otherwise
 */

int is_empty(ISTACK *p)
{
    if ( p == NULL ) {
        return 0;
    }
    return ( p->pos == -1 );
}

/**
 * @brief push one integer onto the stack 
 * @param ISTACK *p the address of the ISTACK data structure
 * @param int item the integer to be pushed onto the stack 
 * @return 0 on success; non-zero otherwise
 */

/**
 * @brief push one integer onto the stack 
 * @param ISTACK *p the address of the ISTACK data structure
 * @param int *item output parameter to save the integer value 
 *        that pops off the stack 
 * @return 0 on success; non-zero otherwise
 */


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

int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    
    if (ptr == NULL) {
        // printf("???%zu\n",max_size);
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
int init_shm_stack(ISTACK *p, int stack_size, int b)
{
    if ( p == NULL || stack_size == 0 ) {
        return 1;
    }

    p->size = b;
    p->pos  = -1;
    printf("b:%d\n",b);

    for(int i=0;i<b;i++){
    p->recv_buf[i].buf = (char *) (&p->recv_buf[i]);
    }
    return 0;
}
int init_EZ_stack(EZTACK *p)
{
    if ( p == NULL) {
        return 1;
    }

    p->count = 0;
    for(int i=0;i<50;i++){
    p->ez[i].buf = (U8 *)p+i;
    }
    return 0;
}
/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv, 
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 

        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;
    return realsize;
}

/**
 * @brief calculate the actual size of RECV_BUF
 * @param size_t nbytes number of bytes that buf in RECV_BUF struct would hold
 * @return the REDV_BUF member fileds size plus the RECV_BUF buf data size
 */
int sizeof_shm_recv_buf(size_t nbytes)
{
    return (sizeof(RECV_BUF) + sizeof(char) * nbytes);
}

/**
 * @brief initialize the RECV_BUF structure. 
 * @param RECV_BUF *ptr memory allocated by user to hold RECV_BUF struct
 * @param size_t nbytes the RECV_BUF buf data size in bytes
 * NOTE: caller should call sizeof_shm_recv_buf first and then allocate memory.
 *       caller is also responsible for releasing the memory.
 */

/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

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
// int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow){	
//     // printf("???\n");
// 	// if (dltotal != 0)
// 	// {
// 		printf("%lf / %lf (%lf %%)\n", dlnow, dltotal, dlnow*100.0 / dltotal);
// 	// }	
// 	return 0;
// }
int producer(int n, struct int_stack *pstack,sem_t *sems,int *img_count)
{
    while(1){
    // sleep(1);
    usleep(1*1000);
    CURL *curl_handle;
    CURLcode res;
    char url[256];
    RECV_BUF recv_buf;
    data_IDAT_p* inf_idat;
    // pid_t pid =getpid();
    recv_buf_init(&recv_buf, BUF_SIZE);
    sem_wait(&sems[0]);
    sprintf(url,"http://ece252-1.uwaterloo.ca:2530/image?img=%d&part=%d",n,*img_count);  
    // printf("img:%s ,%d\n",url,getpid());
    if(*img_count>49){
        sem_post(&sems[0]);
        sem_post(&sems[1]);
        // printf("producer out %d\n",getpid());
        break;
    } 
    img_count[0]++;
    sem_post(&sems[0]);
    // strcpy(url, argv[1]);
    // printf("URL is %s\n", url);

    /* init a curl session */
    curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return 0;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	// curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
	// curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, progress_callback);
	// curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, NULL);
    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl); 
    
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 

    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);
  

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    //printf("lol12\n");
    /* get it! */
    res = curl_easy_perform(curl_handle);
    
    if( res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } 
    // printf("wait sem2 pro\n");
    sem_wait(&sems[2]);
    // printf("success wait sem2 pro%d\n", getpid());
    // int temp=pstack->pos;
    // ++(pstack->pos);
    if ( is_full(pstack) ) {
        // printf("buf full!!! %d\n",getpid());
        // pstack->pos=temp;
        sem_post(&sems[2]);
        // img_count[0]=temp;
        sem_wait(&sems[1]);
        // printf("success out sem1:%d\n",getpid());
        // printf("full and wait sem2 producer:%d\n",getpid());
        sem_wait(&sems[2]);
        // continue;
    }
        // printf("start add png\n");
        if(is_empty(pstack)){
            // printf("start consume\n");
            ++(pstack->pos);
            // printf("pro pos:%d, %d \n",pstack->pos,getpid());
            sem_post(&sems[3]);
        }else{
            ++(pstack->pos);
            //  printf("pro pos:%d, %d \n",pstack->pos,getpid());
        }
        sem_wait(&sems[0]);
        // printf("img_count=%d\n",img_count[0]);

        // *pstack->recv_buf[pstack->pos]=recv_buf;
        // memcpy(pstack->recv_buf[pstack->pos]->buf ,recv_buf.buf,recv_buf.size);

        pstack->recv_buf[pstack->pos].max_size=recv_buf.max_size;

        pstack->recv_buf[pstack->pos].size=recv_buf.size;
        pstack->recv_buf[pstack->pos].seq=recv_buf.seq;

        // strncpy(pstack->recv_buf[pstack->pos].buf,recv_buf.buf,recv_buf.size);
        // printf("%d\n",pstack->pos);
        for(int i=0;i<recv_buf.size;i++){
        
        pstack->recv_buf[pstack->pos].buf[i]=(U8)recv_buf.buf[i];
        printf("%d ",(U8)pstack->recv_buf[pstack->pos].buf[i]);
        }
        // for(int i=0;i<recv_buf.size;i++){
        // printf("%d ",(U8)pstack->recv_buf[pstack->pos].buf[i]);
        // }
        printf("end!\n\n");
    if ( is_full(pstack) ) {
        // printf("buf full!!! %d\n",getpid());
        // pstack->pos=temp;
        sem_post(&sems[0]);
        sem_post(&sems[2]);
        // img_count[0]=temp;
        sem_wait(&sems[1]);
        // printf("success out sem1:%d\n",getpid());
        // printf("full and wait sem2 producer:%d\n",getpid());
        sem_wait(&sems[2]);
        // continue;
    }
        // sem_wait(&sems[0]);
        // printf("what i pushed into pstack(seq)!!!!!!:%d , %d\n",img_count[0],getpid());
        if(*img_count>49){
        sem_post(&sems[0]);
        sem_post(&sems[1]);
        sem_post(&sems[2]);
        // printf("producer out %d\n",getpid());
        break;
        }
        sem_post(&sems[0]);
        sem_post(&sems[2]);
    /* cleaning up */
    curl_easy_cleanup(curl_handle);
    recv_buf_cleanup(&recv_buf);
    }
    // pthread_mutex_unlock(&mutex);
    // pthread_exit(NULL);
    return 0;
}

int consumer(struct int_stack *pstack,sem_t *sems,struct ez_stack *estack,int* asem_count,int *img_count)
{
    while(1){
        // printf("into consumer %d\n",getpid());
        usleep(4*1000);
        sem_wait(&sems[4]);
        if(*asem_count>49){
            sem_post(&sems[3]);
            sem_post(&sems[4]);
            // printf("consumer out %d\n",getpid());
            break;
        }
        sem_post(&sems[4]);
        // printf("wait sem2 cos\n");
        sem_wait(&sems[2]);
        // printf("out consumer wait sem2\n");
        if ( !is_empty(pstack) ) {
            // printf("start pop out\n");
            printf("cos pos: %d;\n",pstack->pos);
            // printf("seq: %d;\n",pstack->recv_buf[pstack->pos].seq);

            int idatnum=0;
            printf("%d ,%d, %d, %d, %d\n",(U8)pstack->recv_buf[pstack->pos].buf[33],(U8)pstack->recv_buf[pstack->pos].buf[34],(U8)pstack->recv_buf[pstack->pos].buf[35],(U8)pstack->recv_buf[pstack->pos].buf[36],(U8)pstack->recv_buf[pstack->pos].buf[37]);
            idatnum=((U8)pstack->recv_buf[pstack->pos].buf[33]<<24)+((U8)pstack->recv_buf[pstack->pos].buf[34]<<16)+((U8)pstack->recv_buf[pstack->pos].buf[35]<<8)+((U8)pstack->recv_buf[pstack->pos].buf[36]);
            // inf_dat[pstack->recv_buf[pstack->pos].seq]->count = idatnum;
            printf("idatnum: %d\n",idatnum);
            U8 * buf=(U8*)malloc(sizeof(U8)*idatnum);

            // inf_dat[]->p_data= pstack->recv_buf[pstack->pos]->;
            // strncpy((char *)inf_dat[pstack->recv_buf[pstack->pos].seq]->p_data,pstack->recv_buf[pstack->pos].buf+41,idatnum);
                // printf("wtf: %d\n", inf_dat[pstack->recv_buf[pstack->pos].seq]->p_data[0]);
            U64 len_inf=0;
            for(int i=0;i<idatnum;i++){
                buf[i]=(U8)pstack->recv_buf[pstack->pos].buf[41+i];
            }
            // char fname[256];
            // sprintf(fname,"%d.txt",pstack->recv_buf[pstack->pos].seq);
            // FILE *fp=fopen(fname,"wb+");
            // printf("seq %d\n",pstack->recv_buf[pstack->pos].seq);
            // printf("wtf: %p\n", estack->ez[pstack->recv_buf[pstack->pos].seq].buf);
            U8 * inf_buf=(U8 *)malloc(9606*sizeof(char));
            int ret = mem_inf(inf_buf, &len_inf, buf, idatnum);
            // fwrite(inf_buf, sizeof(U8), len_inf,fp);
            free(inf_buf);
            // fclose(fp);
            // for(int i=0;i<idatnum;i++){
            //     estack->ez[pstack->recv_buf[pstack->pos].seq].buf[i]=buf[41+i];
            // }
            // printf("zhendejiadea %lu\n",len_inf);
            free(buf);
            if(is_full(pstack)){
                // printf("Release Full!!!!!!!\n");
                (pstack->pos)--;
                // img_count--;
                sem_post(&sems[1]);
                // printf("post the sems1\n");
            }else{
                (pstack->pos)--;
            }

            // printf("after pop pos: %d\n",pstack->pos);
            sem_wait(&sems[4]);
            asem_count[0]++;

            // printf("asem_count=%d\n",asem_count[0]);
            if(*asem_count>49){
            sem_post(&sems[4]);
            sem_post(&sems[2]);
            sem_post(&sems[3]);

            // printf("consumer out %d\n",getpid());
            break;
        }
            sem_post(&sems[4]);

            sem_post(&sems[2]);
        } else {
            // printf("empty stack\n");
            //full! do the algorithm and wait

            sem_post(&sems[2]);
            // printf("wait sem3 cos %d\n",getpid());
            sem_wait(&sems[3]);
            // printf("success out sem3 %d\n",getpid());
            // printf("consumer leaving\n");
        }


    }
    return 0;
}
int main( int argc, char** argv )
{
    if(argc<6){
        printf("no enough arguments");
        return 0;
    }
    int i=0;
    pid_t pid=0;

    int shmid;
    sem_t *sems;
    int* img_count;
    int* asem_count;
    char* fname;
    int shm_size = sizeof_shm_stack(atoi(argv[1]));
    printf("%d\n",shm_size);
    pid_t cpids[atoi(argv[3])+atoi(argv[2])];
    int state;
    double times[2];
    struct timeval tv;

    shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); 
    int shmid_count=shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); 
    int shmid_inf= shmget(IPC_PRIVATE, 50*9606+4, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_sems = shmget(IPC_PRIVATE, sizeof(sem_t) * 4, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_asem=shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); 
    if (shmid == -1 || shmid_sems == -1) {
        perror("shmget");
        abort();
    }

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    struct int_stack *pstack;   
    struct ez_stack *estack;  
    pstack = shmat(shmid, NULL, 0);
    estack=shmat(shmid_inf,NULL,0);
    sems = shmat(shmid_sems, NULL, 0);
    img_count=shmat(shmid_count,NULL,0);
    asem_count=shmat(shmid_asem,NULL,0);
    // pop_stack=shmat(shmid_pop,NULL,0);
    img_count[0]=0;
    asem_count[0]=0;

    init_shm_stack(pstack, shm_size, atoi(argv[1]));
    init_EZ_stack(estack);
    // init_shm_stack(,50);
    if ( pstack == (void *) -1 || sems == (void *) -1 ) {
        perror("shmat");
        abort();
    }
    if ( sem_init(&sems[0], SEM_PROC, 1) != 0 ) {
        perror("sem_init(sem[0])");
        abort();
    }
    if ( sem_init(&sems[1], SEM_PROC, 0) != 0 ) {
        perror("sem_init(sem[1])");
        abort();
    }
    if ( sem_init(&sems[2], SEM_PROC, 1) != 0 ) {
        perror("sem_init(sem[1])");
        abort();
    }
    if ( sem_init(&sems[3], SEM_PROC, 0) != 0 ) {
        perror("sem_init(sem[1])");
        abort();
    }
    if ( sem_init(&sems[4], SEM_PROC, 1) != 0 ) {
        perror("sem_init(sem[1])");
        abort();
    }
    for ( i = 0;  i < atoi(argv[3])+atoi(argv[2]); i++) {
        
        pid = fork();

        if ( pid > 0 ) {        /* parent proc */
            cpids[i] = pid;
        } else if ( pid == 0&&i<atoi(argv[2]) ) { /* child proc */
            producer(atoi(argv[4]),pstack,sems,img_count);
            break;
        } else if(pid == 0||i==atoi(argv[2])&&i>atoi(argv[2])){
            consumer(pstack,sems,estack,asem_count,img_count);
            break;
        }
        else {
            perror("fork");
            abort();
        }
    }
    if ( pid > 0 ) {            /* parent process */
        for ( i = 0; i < atoi(argv[3])+atoi(argv[2]); i++ ) {
            waitpid(cpids[i], &state, 0);
            if (WIFEXITED(state)) {
                printf("Child cpid[%d]=%d terminated with state: %d.\n", i, cpids[i], state);
            }
        }
    //     FILE * fa=fopen("all","rb");
    // U8* def_buf=malloc(50*9606);
    // fread(def_buf, sizeof(U8),50*9606,fa);
    // U8* idat=malloc(50*9606);
    // U64 len_def=0;
    // int ret = mem_def(idat, &len_def,def_buf,50*9606,Z_DEFAULT_COMPRESSION);
    // fclose(fa);
    // FILE *fp=fopen("all.png","wb+");
    // U8 set1[12]={137,80,78,71,13,10,26,10,0,0,0,13};
    // U8 ihdr[17]={73,72,68,82,0,1,144,0,0,1,44,8,6,0,0,0};
    // U8 crc1[4]={0};
    // U32 ihdr_crc=crc(ihdr,17);
    // for(int i=0;i<4;i++){
    // // fprintf(fa,"%x",(ihdr_crc<<(i*8))>>24);
    // crc1[i]=(ihdr_crc<<(i*8))>>24;
    // }
    // fwrite(set1,sizeof(U8),12,fp);
    // fwrite(ihdr,sizeof(U8),17,fp);
    // fwrite(crc1,sizeof(U8),4,fp);
    // U8 DAT[4]={0};
    //   for(int i=0;i<4;i++){
    //     // fprintf(fa,"%lx",(len_def<<(i*8))>>24);
    //     DAT[i]=(len_def<<(i*8))>>24;
    //   }
    // fwrite(DAT,1,4,fp);

    // U8 *p_d=NULL;
      
    // p_d=malloc(len_def+4);
    // U8 set3[4]={73,68,65,84};
    //     for(int i=0;i<4;i++){
    //     // fprintf(fa,"%x",set3[i]);
    //     p_d[i]=set3[i];
    // }
    // for(int i=0;i<len_def;i++){
    //     // fprintf(fa,"%x",gp_buf_def[i]);
    //     p_d[i+4]=def_buf[i];
    // }
    // fwrite(p_d,1,len_def+4,fp);
    // U32 idat_crc=crc(p_d,len_def+4);
    // U8 dcc[4]={0};
    // for(int i=0;i<4;i++){
    //     // fprintf(fa,"%x",(idat_crc<<(i*8))>>24);
    //     dcc[i]=(idat_crc<<(i*8))>>24;
    // }
    // fwrite(dcc,1,4,fp);
    //   U8 set4[12]={0,0,0,0,73,69,78,68,174,66,96,130};
    //   fwrite(set4,1,12,fp);
    //   free(p_d);
    //   fclose(fp);
    // free(idat);
    // free(def_buf);
        if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            abort();
        }
        times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
        printf("Parent pid = %d: total execution time is %.6lf seconds\n", getpid(),  times[1] - times[0]);
    }
    
    return 0;
}
