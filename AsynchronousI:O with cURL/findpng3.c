/* curl_multi_test.c

   Clemens Gruber, 2013
   <clemens.gruber@pqgruber.com>

   Code description:
    Requests 4 Web pages via the CURL multi interface
    and checks if the HTTP status code is 200.

   Update: Fixed! The check for !numfds was the problem.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <search.h>
#include <curl/multi.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
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
#define MAX_WAIT_MSECS 30*1000 /* Wait max. 30 seconds */
#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
} *RECV_BUF;
typedef struct {
    char* data[MAX_URL];
    int front; 
    int rear; 
    int size;
}Queue;

typedef struct hashEntry
{
    const char* key;
    RECV_BUF value;
    struct hashEntry* next;
} entry;

typedef struct hashTable
{
    entry bucket[100]; 
} table;
table tb;
FILE *fp = NULL;
Queue* url_frontier;
int png_count=0;
int count_max= 50;
Queue* CreateQueue();
int IsFullQ(Queue* q);
void AddQ(Queue* q, char* item);
int IsEmptyQ(Queue* q);
char* DeleteQ(Queue* q);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
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
    // printf("checking url : %s\n", url);
    // printf("hash data: %s\n",(char *)hsearch(item,FIND)->data);
    return 0;
}
int IsFullQ(Queue* q) {
    return (q->size == MAX_URL);
}
 
void AddQ(Queue* q, char* item) {
    if (IsFullQ(q)) {
        // printf("full!\n");
        return;
    }
        q->rear++;
        q->rear %= MAX_URL;
        q->size++;
        // q->data[q->rear] = item;
        // memcpy(q->data[q->rear], item, sizeof(item));
        q->data[q->rear]=strDup(item);
        // printf("added: %s\n", q->data[q->rear]);
        return;

}
 
int IsEmptyQ(Queue* q) {
    return (q->size == 0);
}
 
char* DeleteQ(Queue* q) {
    if(IsEmptyQ(q)) {
        // printf("continue wait %d\n",gettid());
        return NULL;
    }

    q->front++;
    q->front %= MAX_URL; //0 1 2 3 4 5
    q->size--;
    // printf("wait end %d\n",gettid());
    // printf("unlocked\n");  

        // printf("deleted %s\n",q->data[q->front] );
    return q->data[q->front];
}
static size_t cb(char *d, size_t n, size_t l, void *p)
{
    size_t realsize = n*l;
    char* url = (char *)p;
    // RECV_BUF ptr = (RECV_BUF)malloc(sizeof(RECV_BUF));
    // RECV_BUF ptr;
    // recv_buf_init(&ptr,BUF_SIZE);
    // memcpy(ptr.buf + ptr.size, d, realsize); /*copy data from libcurl*/
    // ptr.size += realsize;
    // ptr.buf[ptr.size] = 0; 
    // ptr->buf=(char *)malloc(n*l+1);
    // memcpy(ptr->buf, d, realsize);
    // ptr->size=realsize;
    // ENTRY e;
    // e.key=url;
    // e.data = url;
    // e.data=ptr;
    // hsearch(e,ENTER);

    insertEntry(&tb, url, d);
  /* take care of the data here, ignored in this example */
  return n*l;
}

void init(CURLM *cm, char* url)
{
    RECV_BUF ptr;

    CURL *eh =curl_easy_init();

    insertEntry(&tb, url, url);

    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, cb);
    curl_easy_setopt(eh, CURLOPT_WRITEDATA, (void*)url);
    curl_easy_setopt(eh, CURLOPT_HEADER, 0L);
    curl_easy_setopt(eh, CURLOPT_URL, url);
    curl_easy_setopt(eh, CURLOPT_PRIVATE, url);
    curl_easy_setopt(eh, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(eh, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(eh, CURLOPT_UNRESTRICTED_AUTH, 1L);
    /* max numbre of redirects to follow sets to 5 */
    curl_easy_setopt(eh, CURLOPT_MAXREDIRS, 5L);        
    /* supports all built-in encodings */ 
    curl_easy_setopt(eh, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(eh, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(eh, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    curl_easy_setopt(eh, CURLOPT_USERAGENT, "ece252 lab4 crawler");
    curl_multi_add_handle(cm, eh);
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
            if ( href != NULL && !strncmp((const char *)href, "http", 4) && strstr((char*)href, "#") == NULL && findValueByKey(&tb, href) == NULL) {
                AddQ(url_frontier, (char*)href);
                // printf("href added: %s\n", href);
            }else{
                // printf("a href kicked out\n");
            }
            xmlFree(href);

        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}
int process_html(CURL *curl_handle, RECV_BUF p_recv_buf)
{
    // printf("process html\n");
    char fname[256];
    int follow_relative_link = 1;
    char *url = NULL; 
    pid_t pid =getpid();

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url); 
    // sprintf(fname, "./output_%d.html", pid);
    return 0;
}

int process_png(CURL *curl_handle, RECV_BUF p_recv_buf,char* url)
{
    // printf("jiuju\n");
    pid_t pid =getpid();
    char fname[256];
    char *eurl = NULL;          /* effective URL */
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    if ( eurl != NULL) {
        // printf("The PNG url is: %s\n", eurl);
    }
    if(p_recv_buf->size<100){
        return 0;
    }
    //sem
    // printf("count add to : %d , %d\n",png_count, p_recv_buf->size);
    FILE* pu = fopen("png_urls.txt","a+");
    fwrite(url, sizeof(char),strlen(url),pu);
    fputs("\n",pu);
    fclose(pu);
    png_count++;
    // sprintf(fname, "./output_%d_%d.png", p_recv_buf->seq, pid);
    return 0;
}
int process_data(CURL *curl_handle, RECV_BUF p_recv_buf, char* url)
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
        return process_png(curl_handle, p_recv_buf,url);
    } else {
        // sprintf(fname, "./output_%d", pid);
    }

    // return write_file(fname, p_recv_buf->buf, p_recv_buf->size);
    return 0;
}
int send_multi(CURLM* cm)
{
    CURL *eh=NULL;
    CURLMsg *msg=NULL;
    CURLcode return_code=0;
    int still_running=0,  msgs_left=0;
    int http_status_code;
    const char *szUrl;

    curl_multi_perform(cm, &still_running);

    do {
        int numfds=0;
        int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
        if(res != CURLM_OK) {
            fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
            return EXIT_FAILURE;
        }
        /*
         if(!numfds) {
            fprintf(stderr, "error: curl_multi_wait() numfds=%d\n", numfds);
            return EXIT_FAILURE;
         }
        */
        curl_multi_perform(cm, &still_running);

    } while(still_running);

    while ((msg = curl_multi_info_read(cm, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            eh = msg->easy_handle;

            return_code = msg->data.result;
            if(return_code!=CURLE_OK) {
                // fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                continue;
            }

            // Get HTTP status code
            http_status_code=0;
            szUrl = NULL;
            double size = 0;
            curl_easy_getinfo(eh, CURLINFO_SIZE_DOWNLOAD, &size);
            curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
            curl_easy_getinfo(eh, CURLINFO_PRIVATE, &szUrl);
            if(http_status_code<400) {
                   RECV_BUF buf =(RECV_BUF) malloc(sizeof(RECV_BUF));
                //    buf->buf =(char*) malloc(BUF_SIZE);
                //    memcpy(buf->buf, findValueByKey(&tb, szUrl), )
                buf->buf= strDup(findValueByKey(&tb, szUrl));
                buf->size=(int)size;
                // printf("OK for %s\n", szUrl);
                process_data(eh,buf, szUrl);
                free(buf->buf);
                free(buf);
            } else {

                // fprintf(stderr, "GET of %s returned http status code %d\n", szUrl, http_status_code);
            }

            insertEntry(&tb, szUrl , szUrl);

            curl_multi_remove_handle(cm, eh);
            curl_easy_cleanup(eh);
        }
        else {
            // fprintf(stderr, "error: after curl_multi_info_read(), CURLMsg=%d\n", msg->msg);
        }
    }

    return EXIT_SUCCESS;
}
int main( int argc, char** argv ) 
{
    // CURL *curl_handle;
    // CURLcode res;
    char* seed_url;
    // RECV_BUF recv_buf;
    int t=1;
    char *str = "option requires an argument";
    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    
    char* log=malloc(256);
    int c;
    while ((c = getopt (argc, argv, "t:m:v:")) != -1) {
        switch (c) {
        case 't':
	        t = strtoul(optarg, NULL, 10);
	        if (t <= 0) {
                // fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
            }
            break;
        case 'm':
            count_max = strtoul(optarg, NULL, 10);
            if (count_max > 50) {
                count_max=50;
            }
            break;
        case 'v':
            memcpy(log,optarg,sizeof(optarg));
            fp =fopen(log, "a+"); 
            
            break;
        default:
            return -1;
        }
    }
    CURLM *cm=NULL;
    // strcpy(url, argv[argc-1]);
    seed_url= strDup(argv[argc-1]);
    initHashTable(&tb);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    cm = curl_multi_init();
    url_frontier=CreateQueue();
    int connections = 0;
    init(cm, seed_url);
    if(fp!=NULL){
        fwrite(seed_url, sizeof(char),strlen(seed_url),fp);
        fputs("\n",fp);
    }
    send_multi(cm);
    int tims=0;
    free(seed_url);
    while (png_count<count_max) {

        char* url;
        url=strDup(DeleteQ(url_frontier));

      if (url == NULL) {
          if(connections==0){
              if(times<5){
                // printf("mei url le \n");
                continue;
                tims++;
              }else{
                  break;
              }

          }else{
            send_multi(cm);
            connections = 0;
          }

      } else {
          if(findValueByKey(&tb, url)== NULL){
            if(fp!=NULL){
                fwrite(url, sizeof(char),strlen(url),fp);
                fputs("\n",fp);
            }
            init(cm, url);
            connections += 1;
            if (connections >= t) {
                // printf("run the cm\n");
                send_multi(cm);
                connections = 0;
            } else {
                continue;
            }
          }
      }
    }
    freeHashTable(&tb);
    FreeQueue(url_frontier);
    curl_multi_cleanup( cm );
    curl_global_cleanup();
    if(fp!=NULL){
        fclose(fp);
    }
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("findpng3 execution time: %.6lf seconds\n", times[1] - times[0]);
    return 0;
}
