/*
 * The code is derived from cURL example and paster.c base code.
 * The cURL example is at URL:
 * https://curl.haxx.se/libcurl/c/getinmemory.html
 * Copyright (C) 1998 - 2018, Daniel Stenberg, <daniel@haxx.se>, et al..
 *
 * The xml example code is 
 * http://www.xmlsoft.org/tutorial/ape.html
 *
 * The paster.c code is 
 * Copyright 2013 Patrick Lam, <p23lam@uwaterloo.ca>.
 *
 * Modifications to the code are
 * Copyright 2018-2019, Yiqing Huang, <yqhuang@uwaterloo.ca>.
 * 
 * This software may be freely redistributed under the terms of the X11 license.
 */

/** 
 * @file main_wirte_read_cb.c
 * @brief cURL write call back to save received data in a user defined memory first
 *        and then write the data to a file for verification purpose.
 *        cURL header call back extracts data sequence number from header if there is a sequence number.
 * @see https://curl.haxx.se/libcurl/c/getinmemory.html
 * @see https://curl.haxx.se/libcurl/using/
 * @see https://ec.haxx.se/callback-write.html
 */ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <search.h>
#include <pthread.h>
#include <semaphore.h>
#include "crc.h"     
#include "zutil.h"  
#include "lab_png.h"
#include <sys/time.h>

#define SEED_URL "http://ece252-1.uwaterloo.ca/lab4/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define MAX_URL 1000
#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9
#define _XOPEN_SOURCE
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

typedef struct {
    char* data[MAX_URL];
    int front; 
    int rear; 
    int size;
}Queue;

typedef struct hashEntry
{
    const char* key;
    char* value;
    struct hashEntry* next;
} entry;

typedef struct hashTable
{
    entry bucket[100]; 
} table;
typedef struct argument
{
    int limit;/* data */
    FILE *log;
} arg;

int png_count=0;
sem_t sems[2];
Queue* url_frontier;
table tb;
pthread_cond_t qcond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;

Queue* CreateQueue();
int IsFullQ(Queue* q);
void AddQ(Queue* q, char* item);
int IsEmptyQ(Queue* q);
char* DeleteQ(Queue* q);
htmlDocPtr mem_getdoc(char *buf, int size, const char *url);
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath);
int find_http(char *fname, int size, int follow_relative_links, const char *base_url);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
void cleanup(CURL *curl, RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
CURL *easy_handle_init(RECV_BUF *ptr, const char *url);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, char * url);
void* do_work(void* arg);

void initHashTable(table* t)
{
    int i;
    if (t == NULL)return;

    for (i = 0; i < 100; ++i) {
        t->bucket[i].key = NULL;
        t->bucket[i].value = NULL;
        t->bucket[i].next = NULL;
    }
}

void freeHashTable(table* t)
{
    int i;
    entry* e,*ep;
    if (t == NULL)return;
    for (i = 0; i<100; ++i) {
        e = &(t->bucket[i]);
        while (e->next != NULL) {
            ep = e->next;
            e->next = ep->next;
            free(ep->key);
            free(ep->value);
            free(ep);
        }
    }
}

int keyToIndex(const char* key)
{
    int index , len , i;
    if (key == NULL)return -1;

    len = strlen(key);
    index = (int)key[0];
    for (i = 1; i<len; ++i) {
        index *= 1103515245 + (int)key[i];
    }
    index >>= 27;
    index &= (100 - 1);
    return index;
}

char* strDup(const char* str)
{
    int len;
    char* ret;
    if (str == NULL)return NULL;

    len = strlen(str);
    ret = (char*)malloc(len + 1);
    if (ret != NULL) {
        memcpy(ret , str , len);
        ret[len] = '\0';
    }
    return ret;
}

int insertEntry(table* t , const char* key , const char* value)
{
    int index , vlen1 , vlen2;
    entry* e , *ep;

    if (t == NULL || key == NULL || value == NULL) {
        return -1;
    }

    index = keyToIndex(key);
    if (t->bucket[index].key == NULL) {
        t->bucket[index].key = strDup(key);
        t->bucket[index].value = strDup(value);
    }
    else {
        e = ep = &(t->bucket[index]);
        while (e != NULL) { 
            if (strcmp(e->key , key) == 0) {
                vlen1 = strlen(value);
                vlen2 = strlen(e->value);
                if (vlen1 > vlen2) {
                    // free(e->value);
                    // e->value = (char*)malloc(vlen1 + 1);
                    e->value= (char *)realloc(e->value, vlen1 + 1);
                }
                memcpy(e->value , value , vlen1 + 1);
                return index;  
            }
            ep = e;
            e = e->next;
        } 
        e = (entry*)malloc(sizeof (entry));
        e->key = strDup(key);
        e->value = strDup(value);
        e->next = NULL;
        ep->next = e;
    }
    return index;
}

const char* findValueByKey(const table* t , const char* key)
{
    int index;
    const entry* e;
    if (t == NULL || key == NULL) {
        return NULL;
    }
    index = keyToIndex(key);
    e = &(t->bucket[index]);
    if (e->key == NULL) return NULL;
    while (e != NULL) {
        if (0 == strcmp(key , e->key)) {
            return e->value;   
        }
        e = e->next;
    }
    return NULL;
}

int FreeQueue(Queue *q){
    for(int i = 0; i<q->rear; i++){
        free(q->data[i]);
    }
    free(q);
    return 0;
}
Queue* CreateQueue() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    // for(int i = 0;i<MAX_URL;i++){
    //     q->data[i]=(char *)malloc(256*sizeof(char));
    // }
    q->front = -1;
    q->rear = -1;
    q->size = 0;
    return q;
}
int CheckExist(Queue* q, char * url){
    // pthread_mutex_lock(&qlock);
    // int index = q->front;
    // int i;
    // ENTRY item, *ep;
    // printf("checking url : %s\n", url);
    // item.key=url;
    // ep = hsearch(item,FIND);
    // printf("%s \n",item.key);

    // for (i = 0; i < q->size; i++) {
    //     index++;
    //     index %= MAX_URL;
    //     if(strcmp(q->data[index],url)==0){
    //         // printf("hash exist 1\n");
    //         // printf("hash %s : %s\n", (char *)hsearch(item,FIND)->key,(char *)hsearch(item,FIND)->data);
    //         printf("quens\n");
    //         // pthread_mutex_   unlock(&qlock);
    //         return 1;
    //     }
    // }

    sem_wait(&sems[0]);

    if(findValueByKey(&tb, url)!=NULL){
        // printf("hash data: %s\n",(char *)ep->data);
        // printf("exist\n");
        // sem_post(&sems[0]);
        return 1;
    }
    // printf("checking url : %s\n", url);
    // printf("hash data: %s\n",(char *)hsearch(item,FIND)->data);
    return 0;
}
int IsFullQ(Queue* q) {
    return (q->size == MAX_URL);
}
 
void AddQ(Queue* q, char* item) {
    if (IsFullQ(q)) {
        printf("full!\n");
        return;
    }

    pthread_mutex_lock(&qlock);
    // printf("strlen %d\n",strlen(item));
    if(IsEmptyQ(q)){
        q->rear++;
        q->rear %= MAX_URL;
        q->size++;
        // q->data[q->rear] = item;
        // memcpy(q->data[q->rear], item, sizeof(item));
        // printf("bie %d!\n",q->rear);
        q->data[q->rear]=strDup(item);
        // printf("bieba!\n");
        pthread_mutex_unlock(&qlock);
        pthread_cond_signal(&qcond);
        // printf("added %s\n", item);
        return;
    }else{
        int index = q->front;
        int i;
        for (i = 0; i < q->size; i++) {
            index++;
            index %= MAX_URL;
            // printf("in queen %s\n",q->data[index]);
            if(strcmp(q->data[index],item)==0){
                // printf("hash exist 1\n");
                // printf("hash %s : %s\n", (char *)hsearch(item,FIND)->key,(char *)hsearch(item,FIND)->data);
                // printf("quens\n");
                // pthread_mutex_   unlock(&qlock);
                pthread_mutex_unlock(&qlock);
                return;
            }
        }
        q->rear++;
        q->rear %= MAX_URL;
        q->size++;
        // q->data[q->rear] = item;
        // memcpy(q->data[q->rear], item, sizeof(item));
        q->data[q->rear]=strDup(item);
        pthread_mutex_unlock(&qlock);
        pthread_cond_signal(&qcond);
        // printf("added %s\n", item);
        return;
    }


    // printf("add at %s\n",q->data[q->rear]);
}
 
int IsEmptyQ(Queue* q) {
    return (q->size == 0);
}
 
char* DeleteQ(Queue* q) {
    pthread_mutex_lock(&qlock);
    while(IsEmptyQ(q)) {
        // printf("continue wait %d\n",gettid());
        pthread_cond_wait(&qcond, &qlock);
    }
    q->front++;
    // printf("front: %d\n" , q->front);
    q->front %= MAX_URL; //0 1 2 3 4 5
    q->size--;
    // printf("wait end %d\n",gettid());
    pthread_mutex_unlock(&qlock);
    // printf("unlocked\n");   
    return q->data[q->front];
}
htmlDocPtr mem_getdoc(char *buf, int size, const char *url)
{
    int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | \
               HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    htmlDocPtr doc = htmlReadMemory(buf, size, url, NULL, opts);
    
    if ( doc == NULL ) {
        // fprintf(stderr, "Document not parsed successfully.\n");
        return NULL;
    }
    return doc;
}

xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath)
{
	
    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
        printf("Error in xmlXPathNewContext\n");
        return NULL;
    }
    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == NULL) {
        printf("Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        xmlXPathFreeObject(result);
        // printf("No result\n");
        return NULL;
    }
    return result;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url)
{

    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar*) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;
		
    if (buf == NULL) {
        return 1;
    }

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset (doc, xpath);
    if (result) {
        nodeset = result->nodesetval;
        for (i=0; i < nodeset->nodeNr; i++) {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if ( follow_relative_links ) {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }
            if ( href != NULL && !strncmp((const char *)href, "http", 4) && CheckExist(url_frontier,(char* )href)==0 && strstr((char *)href,"#")==NULL) {
                // printf("href added: %s\n", href);
                // pthread_mutex_unlock(&qlock);
                sem_post(&sems[0]);
                AddQ(url_frontier,(char *)href);
            }else{
                // printf("a href kicked out\n");
                pthread_mutex_unlock(&qlock);
                sem_post(&sems[0]);
            }
            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}
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
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

#ifdef DEBUG1_
    // printf("%s", p_recv);
#endif /* DEBUG1_ */
    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
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
    ptr->seq = -1;              /* valid seq should be positive */
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

void cleanup(CURL *curl, RECV_BUF *ptr)
{
        curl_easy_cleanup(curl);
        // curl_global_cleanup();
        recv_buf_cleanup(ptr);
}
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

/**
 * @brief create a curl easy handle and set the options.
 * @param RECV_BUF *ptr points to user data needed by the curl write call back function
 * @param const char *url is the target url to fetch resoruce
 * @return a valid CURL * handle upon sucess; NULL otherwise
 * Note: the caller is responsbile for cleaning the returned curl handle
 */

CURL *easy_handle_init(RECV_BUF *ptr, const char *url)
{
    CURL *curl_handle = NULL;

    if ( ptr == NULL || url == NULL) {
        return NULL;
    }

    /* init user defined call back function buffer */
    if ( recv_buf_init(ptr, BUF_SIZE) != 0 ) {
        return NULL;
    }
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
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)ptr);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)ptr);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ece252 lab4 crawler");

    /* follow HTTP 3XX redirects */
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    /* continue to send authentication credentials when following locations */
    curl_easy_setopt(curl_handle, CURLOPT_UNRESTRICTED_AUTH, 1L);
    /* max numbre of redirects to follow sets to 5 */
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);
    /* supports all built-in encodings */ 
    curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");

    /* Max time in seconds that the connection phase to the server to take */
    //curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
    /* Max time in seconds that libcurl transfer operation is allowed to take */
    //curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    /* Time out for Expect: 100-continue response in milliseconds */
    //curl_easy_setopt(curl_handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);

    /* Enable the cookie engine without reading any initial cookies */
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
    /* allow whatever auth the proxy speaks */
    curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    /* allow whatever auth the server speaks */
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    return curl_handle;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    char fname[256];
    int follow_relative_link = 1;
    char *url = NULL; 
    pid_t pid =getpid();

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url); 
    // sprintf(fname, "./output_%d.html", pid);
    return 0;
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf,char* url)
{
    pid_t pid =getpid();
    char fname[256];
    char *eurl = NULL;          /* effective URL */
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    if ( eurl != NULL) {
        // printf("The PNG url is: %s\n", eurl);
    }
    if(is_png(p_recv_buf->buf,p_recv_buf->size)==0){
        return 0;
    }
    //sem
    sem_wait(&sems[1]);
    png_count++;
    // printf("count add to : %d , %d\n",png_count, p_recv_buf->size);
    FILE* pu = fopen("png_urls.txt","a+");
    fwrite(url, sizeof(char),strlen(url),pu);
    fputs("\n",pu);
    fclose(pu);
    sem_post(&sems[1]);
    // sprintf(fname, "./output_%d_%d.png", p_recv_buf->seq, pid);
    return 0;
}
/**
 * @brief process teh download data by curl
 * @param CURL *curl_handle is the curl handler
 * @param RECV_BUF p_recv_buf contains the received data. 
 * @return 0 on success; non-zero otherwise
 */

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, char* url)
{
    CURLcode res;
    char fname[256];
    pid_t pid =getpid();
    long response_code;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if ( res == CURLE_OK ) {
	    // printf("Response code: %ld\n", response_code);
    }

    if ( response_code >= 400 ) { 
    	// fprintf(stderr, "Error.\n");
        return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    	// printf("Content-Type: %s, len=%ld\n", ct, strlen(ct));
    } else {
        // fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }

    if ( strstr(ct, CT_HTML) ) {
        return process_html(curl_handle, p_recv_buf);
    } else if ( strstr(ct, CT_PNG) ) {
        sem_post(&sems[0]);
        return process_png(curl_handle, p_recv_buf,url);
    } else {
        // sprintf(fname, "./output_%d", pid);
    }

    // return write_file(fname, p_recv_buf->buf, p_recv_buf->size);
    return 0;
}

int main( int argc, char** argv ) 
{
    // CURL *curl_handle;
    // CURLcode res;
    char* url;
    // RECV_BUF recv_buf;
    int t=1;
    char *str = "option requires an argument";
    int m=50;
    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    
    char* log=malloc(256);
    int c;
    arg *a;
    a=malloc(sizeof(arg));
    while ((c = getopt (argc, argv, "t:m:v:")) != -1) {
        switch (c) {
        case 't':
	        t = strtoul(optarg, NULL, 10);
	        // printf("option -t specifies a value of %d.\n", t);
	        if (t <= 0) {
                // fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
            }
            break;
        case 'm':
            m = strtoul(optarg, NULL, 10);
	    // printf("option -m specifies a value of %d.\n", m);
            if (m > 50) {
                m=50;
            }
            break;
        case 'v':
            memcpy(log,optarg,sizeof(optarg));
            FILE *fp =fopen(log, "w+"); 
            a->log=fp;
            printf("option -m specifies a value of %s.\n", log);
            break;
        default:
            return -1;
        }
    }
    
    // strcpy(url, argv[argc-1]);
    url= strDup(argv[argc-1]);
    // hcreate(MAX_URL);
    initHashTable(&tb);
    url_frontier=CreateQueue();
    AddQ(url_frontier,url);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    pthread_t * p_tids=malloc(sizeof(pthread_t)*t);

    a->limit=m;
    for(int i=0;i<2;i++){
        sem_init(&sems[i],0,1);
    }
    for(int i=0;i<t;i++){
        pthread_create(p_tids + i, NULL, do_work,(void* )a );
    }
    for (int i=0; i<t; i++) {
        pthread_join(p_tids[i], NULL);
    }
    for(int i=0;i<2; i++){
        sem_destroy(&sems[i]);
    }
    curl_global_cleanup();
    free(p_tids);
    free(log);
    free(a);
    freeHashTable(&tb);
    if(a->log!=NULL){
        fclose(a->log);
    }
    FreeQueue(url_frontier);
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("findpng2 execution time: %.6lf seconds\n", times[1] - times[0]);
    return 0;
}

void* do_work(void* argu){
    arg * get = (arg* )argu;
    int limit = get->limit;
    while(1){
        // printf("stoped1 %d\n",gettid());
        sem_wait(&sems[1]);
        if(png_count>=limit){
        sem_post(&sems[1]);
        AddQ(url_frontier, "end");
        // printf("thread end %d\n", gettid());
        return NULL;
        }

        sem_post(&sems[1]);

        char* url;
        // char * geturl=DeleteQ(url_frontier);
        // url=malloc(sizeof(char)*sizeof(geturl));
        // memcpy(url, geturl, sizeof(geturl));
        url=strDup(DeleteQ(url_frontier));
        sem_wait(&sems[1]);
        if(png_count>=limit){
        sem_post(&sems[1]);
        AddQ(url_frontier, "end");
        // printf("thread end %d\n", gettid());
        return NULL;
        }
        sem_post(&sems[1]);
        // printf("stoped2%d\n",gettid());
        sem_wait(&sems[0]);
        insertEntry(&tb,url,url);
        if(get->log!=NULL){
            fwrite(url,sizeof(char),strlen(url),get->log);
            fputs("\n",get->log);
        }
        sem_post(&sems[0]);
        // printf("process : %s\n",url);
        // printf("q front: %d, q rear: %d, q data:%s\n", url_frontier->front, url_frontier->rear, url_frontier->data[url_frontier->front]);
        CURL *curl_handle;
        CURLcode res;
        RECV_BUF recv_buf;
        curl_handle = easy_handle_init(&recv_buf, url);
        
        if ( curl_handle == NULL ) {
            // fprintf(stderr, "Curl initialization failed. Exiting...\n");
            abort();
        }

        /* get it! */
        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK || strstr(url,"#")) {
            // fprintf(stderr, "curl_easy_perform() failed: %s, %s\n", curl_easy_strerror(res), url);

            // printf("successfull insert hash %s\n",url);

            // cleanup(curl_handle, &recv_buf);
            // continue;
            // exit(1);
        } else {
            // ENTRY item;
            // item.key=url;
            // item.data=url;
            // memcpy(item.key, url, 256);

            // hsearch(item,ENTER);

            /* process the download data */
            


            process_data(curl_handle, &recv_buf,url);

            /* cleaning up */

        }
        cleanup(curl_handle, &recv_buf);
    }
}
