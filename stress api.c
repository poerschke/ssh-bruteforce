#include <arpa/inet.h>
#include <assert.h>
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
#include <ctype.h>
#include <openssl/ssl.h>



/* -------------------------------------------------------------------

Autor: Douglas Poerschke Rocha
Versão: 0.3
Data: 11/11/2022


Poc desenvolvido para estressar chamadas de API

Compilar com GCC:
gcc -o poc poc.c -l pthread -lssl -lcrypto

Mode de usar:
./poc <method> <api path> <threads> <user> <senha> <time>

Exemplo:
./poc GET /profile 10 user@dominio.com.br senha 60

------------------------------------------------------------------- */

// cores
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

// variaveis globais
char 	user[256], senha[256], method[10], api[256], header[5000], jwt[1024];
char 	dominio[] = "dominio.com";
char 	ip[] = "192.168.1.1";
int 	porta = 443;
int 	threads = 0;
int 	time_count = 0;
int	    stop = 0;
int	    requests = 0;
int	    timeouts = 0;
int	    success = 0;
int	    error = 0;
pthread_mutex_t lock;
pthread_cond_t cond;


// funções
void * 	worker();
void * 	time_controller();
void 	GET(char *path);
void 	POST(char *path, char *data, char *ret);
void 	headers_GET(char custom[]);
void 	headers_POST(char data[], char custom[]);
void 	get_JWT();
void 	clean_string(char *string);
void 	grava(char msg[]);
char ** str_split(char* a_str, const char a_delim);
int 	connecta();
SSL * 	attach(int con);



int main(int argc, char *argv[]){
    int ti;
    
    if (argc != 7){
    	printf("Modo de uso:\n%s <method> <api path> <threads> <user> <pass> <time>\nexemplo:\n%s GET /profile 150 user@dominio.com senha 60\n", argv[0], argv[0]);
    	return 1;
    }

    strcpy(method, argv[1]);
    strcpy(api, argv[2]);
    threads = atoi(argv[3]);
    strcpy(user, argv[4]);
    strcpy(senha, argv[5]);
    time_count = atoi(argv[6]);

    // loga e pega token jwt    
    get_JWT();
    printf(BOLDBLUE "Token: " GREEN "%s\n\n" BOLDBLUE, jwt);
    
    
    pthread_t th[threads+1];
    //mutex pra evitar race condiction no contador de requests
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL); 
    
     
    for(ti = 0; ti <threads; ti++){
            pthread_create(&th[ti], NULL, worker, NULL);
            printf(BOLDBLUE "Thread [" GREEN "%d" BOLDBLUE "] criado\r", ti);
            fflush(stdout);
    }
    // cria thread do controlador de tempo
    pthread_create(&th[threads+1], NULL, time_controller, NULL);
    printf("\nIniciando requests\n");
    
    for(ti = 0; ti <threads; ti++){
            pthread_join(th[ti], NULL);
    }
    return 0;
}


void * worker(){
    char auth[1024];
    char ret[5000];
    
    sprintf(auth, "Authorization: Bearer %s", jwt);
    
    if(strstr(method, "POST") != NULL)
        headers_POST("", auth);
    else
        headers_GET(auth);
    
    
    while(1){
    	if (stop == 0){
	    if(strstr(method, "POST") != NULL)
		POST(api, "", ret);
	    else
		GET(api);
	}
	else
	    return 0;     
    }
}


int connecta(){
    int fd, n, sock;
    fd_set sockets;
    char pt[10];

    struct timeval tv;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof hints);

    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    sprintf(pt, "%d", porta);
    getaddrinfo(dominio, pt, &hints, &res);

    fd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
    connect(fd,res->ai_addr,res->ai_addrlen);
    return fd;
}  


void headers_GET(char custom[]){
    if(strlen(custom) < 2)
	sprintf(header, "Host: %s\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.84 Safari/537.36\r\nAccept-Encoding: gzip, deflate, br\r\nAccept: application/json\r\nConnection: keep-alive\r\n\r\n", dominio);
    else
        sprintf(header, "Host: %s\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.84 Safari/537.36\r\nAccept-Encoding: gzip, deflate, br\r\nAccept: application/json\r\nConnection: keep-alive\r\n%s\r\n\r\n", dominio, custom);
		
}


void headers_POST(char data[], char custom[]){
    if(strlen(custom) < 2)
	sprintf(header, "Host: %s\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.84 Safari/537.36\r\nAccept-Encoding: gzip, deflate, br\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", dominio, strlen(data), data);
    else
	sprintf(header, "Host: %s\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.84 Safari/537.36\r\nAccept-Encoding: gzip, deflate, br\r\nAccept: application/json\r\nContent-Type: application/json\r\n%s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", dominio, custom, strlen(data), data);
		
}


SSL * attach(int con){
    SSL_load_error_strings();
    SSL_library_init();
    SSL_CTX *ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    SSL *conn = SSL_new(ssl_ctx);
    SSL_set_fd(conn, con);
    int err = SSL_connect(conn);
    if (err != 1){
        printf("erro\n");
        abort();
    }
    return conn;
}


void GET(char *path){
    int con, n;
    SSL *conn;
    char request[4000], buffer[5000];
	
    con = connecta();
    if(con != -1){
        conn = attach(con);
        sprintf(request, "GET %s HTTP/1.1\r\n%s", path, header);
        SSL_write(conn, request, strlen(request));
	    n = SSL_read(conn, buffer, sizeof(buffer)-1);
	    buffer[n] = '\0'; 
        shutdown(con, SHUT_RDWR);
	    close(con);

        pthread_mutex_lock(&lock);
        requests++;
        if(strstr(buffer, "success") != NULL)
            success++;
        else{
            error++;
            grava(buffer);
        }
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lock); 	
    }
    else{
        pthread_mutex_lock(&lock);
        timeouts++;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lock);    
    }   
}


void POST(char *path, char *data, char *ret){
    int con, n;
    SSL *conn;
    char request[4000], buffer[5000];
	
    con = connecta();
    if(con != -1){
        conn = attach(con);
        //headers_POST(data, "");
        sprintf(request, "POST %s HTTP/1.1\r\n%s", path, header);
        SSL_write(conn, request, strlen(request));
        n = SSL_read(conn, buffer, sizeof(buffer)-1);
        buffer[n] = '\0'; 
        strcpy(ret, buffer);
        shutdown(con, SHUT_RDWR);
        close(con);


        pthread_mutex_lock(&lock);
        requests++;
        if(strstr(buffer, "success") != NULL)
            success++;
        else{
            error++;
            grava(buffer);
        }
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lock);        
    }
    else{
        pthread_mutex_lock(&lock);
        timeouts++;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lock);        
    }
}


void get_JWT(){
    char ret[5000];
    char str[1024];

    sprintf(str, "{\n\"username\": \"%s\", \n\"password\": \"%s\"\n}", user, senha);
    POST("/auth/token", str, ret);

    if(strstr(ret, "Invalid credentials.") != NULL){
	    printf("Credenciais inválidas\n");
	    exit(1);
    }
    else if(strstr(ret, "success") != NULL){
	    clean_string(ret);
	    char** tokens;
	    tokens = str_split(ret, '"');
	    if (tokens){
            strcpy(jwt, *(tokens + 11));
	        free(tokens);
    	}
    }
}



void clean_string(char *string){
    int i, x=0;
    char *newstring;

    newstring = (char *)malloc(strlen(string)-1);

    for(i=0; i< strlen(string); i++){
	if (string[i] != '\n' && string[i] != '\r' ){
	    newstring[i-x] = string[i];
	}
	else
            x++;
    }

    newstring[i-x] = '\0';
    strcpy(string, newstring);
}


char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    while(*tmp){
        if (a_delim == *tmp){
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    count += last_comma < (a_str + strlen(a_str) - 1);
    count++;

    result = malloc(sizeof(char*) * count);
    if (result){
        size_t idx  = 0;
        char* token = strtok(a_str, delim);
        while (token){
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }
    return result;
}


void * time_controller(){
    int start_time = (int)time(NULL);
    int end_time = start_time + time_count;
    int current_time = 0;
    float rs = 0.0;
    while(1){
    	if ((int)time(NULL) > end_time)	{
            stop = 1;
            break;
    	}
    	else {
            sleep(0.1);
            current_time = (int)time(NULL);
            rs = (float) requests / (current_time - start_time);
            printf("\r[" GREEN "%d" BOLDBLUE "] Requests: [" GREEN "%d" BOLDBLUE "] R/sec: [" YELLOW "%.2f" BOLDBLUE "] Timeouts: [" RED "%d" BOLDBLUE "] Success: [" GREEN "%d" BOLDBLUE "] Error: [" RED "%d" BOLDBLUE "]   ", end_time - current_time + 1, requests, rs, timeouts, success, error);
            fflush(stdout);

    	}
    }
}



void grava(char msg[]){
    FILE *fp;
    if((fp = fopen("error.txt", "a+")) != NULL){
        fprintf(fp, "%s\n", msg);
        if(fclose(fp)){
            printf("Erro ao fechar error.txt\n");
        }
    }
}    

