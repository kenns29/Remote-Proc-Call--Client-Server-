#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>
#include <math.h>
#include <ctype.h>
#define FALSE 0
#define TRUE 1
#define SEND_MAX 100000

/** MODIFY THIS TO TEST THE BUSY CASE **/
int BUSY_PERCENT = 50;
/***************************************/

pthread_mutex_t sender_mutex;
pthread_mutex_t handler_mutex;
/* arguments of send_request thread function */
typedef struct{
		char *full_server_name; /* un-parsed server name */
		int sending_sockfd;		
		char **own_services;
		char *own_server_name;
		struct ServiceList *service_list;
}Thread_args;

/* arguments of the handle_request thread function */
typedef struct{
	int sockfd;
	char **own_service_table;
	struct ServiceList *service_list;
	char *own_server_name;
}HandlerArgs;

void error(const char *s){
	fprintf(stderr, "%s\nerrno: %s\n", s, strerror(errno));
	exit(1);
}
/******************/
/* the node class */
/******************/
struct node{
	char *serv_name;
	int port;
	struct node* pre;
	struct node* next;
};

/* constructors of node */
struct node* new_empty_node(){
	struct node* n = (struct node*)malloc(sizeof(struct node));
	n->serv_name = (char*)malloc(500*sizeof(char));
	n->serv_name[0] = '\0';
	n->port = 0;
	n->pre = NULL;
	n->next = NULL;
	return n;
}
/* parameter: s must be the form [server_name]:[port_number] */
struct node* new_node(char *s){
  struct node* n = (struct node*)malloc(sizeof(struct node));
	char temp[strlen(s)+1];
	strcpy(temp, s);
	char *str = temp;
	char *tok = strchr(temp, ':');
	*tok = '\0';
	++tok;
	n->serv_name = (char*)malloc(500*sizeof(char));
	strcpy(n->serv_name, str);
	n->port = (int)strtol(tok, NULL, 10);
	n->pre = NULL;
	n->next = NULL;
	return n;
}
/*************************/
/* end of the node class */
/*************************/

/************************/
/* the linkedlist class */
/************************/
struct linkedlist{
	struct node* head;
	struct node* tail;
};
/* constructors of linkedlist */
struct linkedlist* new_empty_linkedlist(){
	struct linkedlist* l = (struct linkedlist*)malloc(sizeof(struct linkedlist));
	l->head = NULL;
	l->tail = NULL;
	return l;
}
struct linkedlist* new_linkedlist(char *s){
	struct linkedlist* l = (struct linkedlist*)malloc(sizeof(struct linkedlist));
	l->head = new_node(s);
	l->tail = l->head;
	return l;
}
/* member functons */
void add_node(struct linkedlist *l, char *s){
	if (l->head == NULL){
		l->head = new_node(s);
		l->tail = l-> head;
	}
	else if(l->head->next == NULL){
		l->head->next = new_node(s);
		l->tail = l->head->next;
		l->tail->pre = l->head;
	}
	else{
		l->tail->next = new_node(s);
		l->tail->next->pre = l->tail;
		l->tail = l->tail->next;
	}
}
/*******************************/
/* end of the linkedlist class */
/*******************************/

/*********************/
/* the service class */
/*********************/
struct service{
	struct linkedlist *server_list;
	char *function;
	struct service *pre;
	struct service *next;
};
/* constructor for service class */
/* parameter: fun must be the form of [proc] [param] [param] ... */
/*            serv must be the form [server_name]:[port_number] */
struct service* new_service(char *fun, char *serv){
	struct service *s = (struct service*)malloc(sizeof(struct service));
	s->function = (char *)malloc(100*sizeof(char));
	strcpy(s->function, fun);
	s->server_list = new_linkedlist(serv);
	s->pre = NULL;
	s->next = NULL;
	return s;
}
struct service* new_empty_service(char *fun){
	struct service *s = (struct service*)malloc(sizeof(struct service));
	s->function = (char *)malloc(100*sizeof(char));
	strcpy(s->function, fun);
	s->server_list = new_empty_linkedlist();
	s->pre = NULL;
	s->next = NULL;
	return s;
}
/************************/
/* end of service class */
/************************/

/*************************/
/* the ServiceList class */
/*************************/
struct ServiceList{
	struct service *head;
	struct service *tail;
};
/* constructors for ServiceList class */
struct ServiceList* new_empty_ServiceList(){
	struct ServiceList *s = (struct ServiceList*)malloc(sizeof(struct ServiceList));
	s->head = NULL;
	s->tail = NULL;
	return s;
}

/* parameter: fun must be the form of [proc] [param] [param] ... */
/*            serv must be the form [server_name]:[port_number] */
struct ServiceList* new_ServiceList(char *fun, char *serv){
	struct ServiceList *s = (struct ServiceList*)malloc(sizeof(struct ServiceList));
	s->head = new_service(fun, serv);
	s->tail = s->head;
	return s;
}
/* member functions for ServiceList class */
void add_service(struct ServiceList* sl, char *fun, char *serv){
	if(sl->head == NULL){
		sl->head = new_service(fun, serv);
		sl->tail = sl->head;
	}
	else if(sl->head->next == NULL){
		sl->head->next = new_service(fun, serv);
		sl->tail = sl->head->next;
		sl->tail->pre = sl->head;
	}
	else{
		sl->tail->next = new_service(fun, serv);
		sl->tail->next->pre = sl->tail;
		sl->tail = sl->tail->next;
	}
}
void add_empty_service(struct ServiceList *sl, char *fun){
	if(sl->head == NULL){
		sl->head = new_empty_service(fun);
		sl->tail = sl->head;
	}
	else if(sl->head->next == NULL){
		sl->head->next = new_empty_service(fun);
		sl->tail = sl->head->next;
		sl->tail->pre = sl->head;
	}
	else{
		sl->tail->next = new_empty_service(fun);
		sl->tail->next->pre = sl->tail;
		sl->tail = sl->tail->next;
	}
}
/****************************/
/* end of ServiceList class */
/****************************/

void print_promt_message(int, char**, char*);
void parse_server_name(char *, char*, char*);
void contact_all_other_servers(char*, int, char**, char**, struct ServiceList*);
void send_request(void *);
void send_server_info(int, char*, char**);
int is_server_request(char*);
void handle_request(void*);
void handle_server_request(int, char*, char**, struct ServiceList*, char*);
void handle_client_request(int, char*, char**, struct ServiceList*, char*);
void get_full_server_name(char*, char*, int);
void send_service_list_info(int, char*, char**, struct ServiceList*);
void recv_service_list_info(int, struct ServiceList*);
void send_result_to_client(int sockfd, char*, char**, int*, int, char**, int, char *, struct ServiceList*, char* func_name);
char *next_server(char*, struct ServiceList*, char **, int);
char *recv_once(int);
void to_l_case(char*);
void send_unknown_message(int, char*, char*);
void send_busy_message(int, char*, char*);
void send_mismatch_message(int, char*);
void send_out_message(int);
void send_result_message(int, char*, int);
void check_arguments(int, char**, int*, char***, char**, char**);
/* service procedures */
int get_int(int); 					/* service# 0 */
int add(int, int);					/* service# 1 */
int sub(int, int);					/* service# 2 */
int mul(int, int);					/* service# 3 */
int a(int, int);						/* service# 4 */	
int b(int, int);						/* service# 5 */
int c(int, int);						/* service# 6 */
int d(int, int);						/* service# 7 */
int e(int, int);						/* service# 8 */
int f(int, int);						/* service# 9 */
#define SERVICE_TABLE_SIZE 10
int own_service_table_size;
/* main */
int main(int argc, char **argv){

  signal(SIGCHLD, SIG_IGN);
	int i = 0;
	
	/*********************************************/
	/* Service Table that holds all the services */
	/*********************************************/
	char *service_table[SERVICE_TABLE_SIZE];
	for(i = 0; i<SERVICE_TABLE_SIZE; i++){
		service_table[i] = (char*)malloc(500*sizeof(char));
	}
	/* copy each service to the table */
	strcpy(service_table[0], "get_int int");
	strcpy(service_table[1], "add int int");
	strcpy(service_table[2], "sub int int");
	strcpy(service_table[3], "mul int int");
	strcpy(service_table[4], "a int int");
	strcpy(service_table[5], "b int int");
	strcpy(service_table[6], "c int int");
	strcpy(service_table[7], "d int int");
	strcpy(service_table[8], "e int int");
	strcpy(service_table[9], "f int int");  
	/************************/
	/* end of Service Table */
	/************************/
	/******************************************************/
	/* service table that hold its own available services */
	/******************************************************/
	char *own_service_table[SERVICE_TABLE_SIZE];
	for(i = 0; i<SERVICE_TABLE_SIZE; i++){
		own_service_table[i] = (char*)malloc(500*sizeof(char));
	}
	/* copy each service to the own service table (different in each server) */
	int new_argc;
	char **new_argv;
	check_arguments(argc, argv, &new_argc, &new_argv, service_table, own_service_table); 
	/************************/
	/* end of service table */
	/************************/
	
	/****************************************/
	/* create service list of other servers */
	/****************************************/
	struct ServiceList *service_list = new_empty_ServiceList();
	/*******************************/
	/* end of creating ServiceList */
	/*******************************/
	
	/* declaration */
	int listening_sockfd, client_sockfd;
  socklen_t client_sock_len;
 	char* buff;
  struct sockaddr_in server_address, client_address;
  int port_no = 4000;


  /* create listening socket */
  if ( (listening_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
  	error("error in creating listening socket");
  }
  
  /* bind option */
  int yes = 1;
  if (setsockopt(listening_sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
    error("error in setsockopt");
	} 
	
	server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  //server_address.sin_addr.s_addr = inet_addr("192.168.1.120");
  server_address.sin_port = htons(port_no);
  
  /* bind */
  while( (bind(listening_sockfd, (struct sockaddr *)&server_address, (socklen_t)sizeof(server_address))) == -1 ){
    ++port_no;
    server_address.sin_port = htons(port_no);
    if (port_no >1000000){
      error("bind error.\n");
    }
  }
 
  pthread_mutex_init(&handler_mutex, NULL);
  pthread_mutex_init(&sender_mutex, NULL);
  
  /******************************/
  /* get the name of the server */
  /******************************/
  char host_info[SEND_MAX];
  char service_info[SEND_MAX];
  //getnameinfo((struct sockaddr*)&server_address, sizeof(server_address),host_info, sizeof host_info, service_info, sizeof service_info, 0);
  if(gethostname(host_info, SEND_MAX)!=0){
  	error("error in gethostname");
  }
  to_l_case(host_info);
  /**********************/
  /* end of getnameinfo */
  /**********************/
  
  /***********************/
  /* print promt message */
  /***********************/
  print_promt_message(port_no, own_service_table, host_info);
  /*************/
  /* end promt */
  /*************/
  /*********************************************************/
  /* contact and send information to all the other servers */
  /*********************************************************/
  /* params for contact_all_other_servers */
  char *own_server_name = (char*)malloc(500*sizeof(char));
  char *port = (char*)malloc(100*sizeof(char));
  sprintf(port, "%d", port_no);
  //strcpy(own_server_name, "localhost:");
  strcpy(own_server_name, host_info);
  strcat(own_server_name, ":");
  strcat(own_server_name, port);
  /* contact_all_other_servers */
	contact_all_other_servers(own_server_name, new_argc, new_argv, own_service_table, service_list);
	free(own_server_name);
	free(port);
	/************************************/
	/* end of contact_all_other_servers */
	/************************************/
	
  /* listening to client */
  if (listen(listening_sockfd, 20) == -1)
      error("error in listen\n");
  printf("Listening on port: %d\n\n", port_no);
  
  while(TRUE){
  	client_sock_len = sizeof(client_address);
  	client_sockfd = accept(listening_sockfd, (struct sockaddr*)&client_address, &client_sock_len);
  	printf("a connection established\n");
  	
  	/********************************/
  	/* thread to handle the request */
  	/********************************/
  	pthread_t tid;
  	pthread_attr_t attr;
		pthread_attr_init(&attr);
  	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  	HandlerArgs *handlerArgs = (HandlerArgs*)malloc(sizeof(HandlerArgs));
  	handlerArgs->sockfd = client_sockfd;
  	handlerArgs->own_service_table = own_service_table;
  	handlerArgs->service_list = service_list;
  	handlerArgs->own_server_name = (char*)malloc(1000*sizeof(char));
  	char *port = (char*)malloc(1000*sizeof(char));
  	sprintf(port, "%d", port_no);
  	//strcpy(handlerArgs->own_server_name, "localhost:");
  	strcpy(handlerArgs->own_server_name, host_info);
  	strcat(handlerArgs->own_server_name, ":");
  	strcat(handlerArgs->own_server_name, port);
  	free(port);
  	/* create the thread */
  	pthread_create(&tid, &attr, (void*)handle_request, (void*)handlerArgs);
  	/*****************************/
  	/* end of the handler thread */
  	/*****************************/
  } 
  
  close(listening_sockfd);
	return 0;
}
/* end main */

/* print promt message */
void print_promt_message(int port, char **own_service_table, char *host_info){
	printf("To simulate the Busy Case, please enter the percentage that the server will be busy. Ex: 50 for 50 percent, 100 for 100 percent:\n");
	scanf("%d", &BUSY_PERCENT);
	printf("=============================================\n");
	printf("Server Starts.\n");
	printf("Server name: %s:%d\n", host_info, port);
	printf("Here is a list of services this server provides:\n\n");
	int i=0;
	for(i=0; i<own_service_table_size; i++){
		printf("%s\n", own_service_table[i]);
	}
	printf("\nThis server has %d percent chance to be busy.\n", BUSY_PERCENT);
	printf("=============================================\n");
}
/* parse the server name */
/* param: s must be the form [server_name]:[port_number]
					n is the [server_name] (return value)
					port is the [port_number] in integer (return value) */
void parse_server_name(char *s, char *n, char *port){
	char *saveptr1;
	char *temp = (char*)malloc( (strlen(s)+1)*sizeof(char) );
	strcpy(temp, s);
	char *str = strtok_r(temp, ":", &saveptr1);
	//n = (char*)malloc( (strlen(str)+1)*sizeof(char) );
	
	strcpy(n, str);
	to_l_case(n);
	str = strtok_r(NULL, ":", &saveptr1);
	//port = (char*)malloc( (strlen(str)+1)*sizeof(char) );
	if (str == NULL){
		error("Error:addressses must be the form [server_name]:[port_number]");
	}
	strcpy(port, str);
	free(temp);
}

/* contact and send request to all other servers */
/* param: argc is the argc of the main
				  argv is the argv of the main
				  own_services is the server's own service table */
void contact_all_other_servers(char *own_server_name, int argc, char **argv, char **own_services, struct ServiceList *service_list){
	if (argc < 2) return;
	
	int number_of_request = argc-1;			
	char **sl = (char**)malloc( number_of_request*sizeof(char*));
	int i = 0;
	for (i=0; i< number_of_request; i++){
		sl[i] = (char*)malloc( ( strlen( argv[i+1] ) + 1 ) * sizeof(char) );
		strcpy(sl[i], argv[i+1]);
	}
	
	int sending_sockfds[number_of_request];
	
	pthread_attr_t attr;
	pthread_t threadID[number_of_request];
	pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i<number_of_request; i++){
		/* creating the sending socket */
		sending_sockfds[i] = socket(AF_INET, SOCK_STREAM, 0);
		/* set up the arguments */
		Thread_args *thread_args = (Thread_args*)malloc(sizeof(Thread_args));
		thread_args->full_server_name = (char*)malloc( ( strlen(sl[i]+1) * sizeof(char) ) );
		strcpy(thread_args->full_server_name, sl[i]);
		thread_args->sending_sockfd = sending_sockfds[i];
		thread_args->own_services = own_services;
		thread_args->own_server_name = (char*)malloc( (strlen(own_server_name)+1)*sizeof(char) );
		strcpy(thread_args->own_server_name, own_server_name);
		thread_args->service_list = service_list;
		/* create the thread to call send_request procedure */
		pthread_create(&(threadID[i]), &attr, (void*)send_request, (void *)thread_args); 
	}
	/*void *status;
	for(i = 0; i<number_of_request; i++){
		pthread_join(threadID[i], &status);
	}*/
	
	/* free */
	for(i=0; i<number_of_request; i++){
		free(sl[i]);
	}
	free(sl);
}
/* thread function for sending other server request */
/* param: arg is the struct Thread_args */
void send_request(void *arg){
	Thread_args *thread_args = (Thread_args*) arg;
	char *port = (char*)malloc(500*sizeof(char));
	char *server_name = (char*)malloc(500*sizeof(char)); /* parsed server name */
	parse_server_name(thread_args->full_server_name, server_name, port);
	if ( strcmp(server_name, "localhost")==0 ){
		strcpy(server_name, "127.0.0.1");
	}
	struct addrinfo hints;
	struct addrinfo *servinfo;
	memset(&hints, '0', sizeof(hints));
	//printf("%s\n", server_name);
	//printf("%s\n", port);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;
	
	/* getaddrinfo */
	int status = getaddrinfo(server_name, port, &hints, &servinfo); 
	if (status != 0){
		printf("%s\n", gai_strerror(status));
		error("error in getaddrinfo");
	}
	else{
		printf("connecting to a server...\n");
	}
	/* connect */
	if( connect(thread_args->sending_sockfd, servinfo->ai_addr, servinfo->ai_addrlen) != 0){
		error("error in connect");
	}
	else{
		printf("successfully connected to a remote server.\n");
	}
	
	/* send */
	send_server_info(thread_args->sending_sockfd, thread_args->own_server_name, thread_args->own_services);
	recv_service_list_info(thread_args->sending_sockfd, thread_args->service_list);
	/* free */
	free(server_name);
	free(port);
	freeaddrinfo(servinfo);
	free(thread_args->full_server_name);
	free(thread_args->own_server_name);
	free(arg);
	pthread_exit(NULL);
}

/* update the service list */
void recv_service_list_info(int sockfd, struct ServiceList *service_list){
	char *buff = recv_once(sockfd);
	char *saveptr1, *saveptr2;
	char *str, *substr;
	pthread_mutex_lock(&handler_mutex); /* lock */
	/*printf("-----------------\n");
	printf("%s", buff);
	printf("-----------------\n");*/
	
	str = strtok_r(buff, "\n", &saveptr1);
	char server_name[strlen(str)+1];
	strcpy(server_name, str);
	str = strtok_r(NULL, "\n", &saveptr1);
	
	while( strcmp(str, "TABLE") != 0 ){
		substr = strtok_r(str, "&", &saveptr2);
		struct service *service_iter;
		for(service_iter = service_list->head; service_iter != NULL; service_iter = service_iter->next){
			if( strcmp(service_iter->function, substr)==0 ){
				break;			
			}
		}
		if(service_iter == NULL){
			add_empty_service(service_list, substr);
			service_iter = service_list->tail;
		}
		char *s = (char*)malloc(1000*sizeof(char));
		char *port = (char*)malloc(1000*sizeof(char));
		while( (substr=strtok_r(NULL, "&", &saveptr2)) != NULL){
			struct node *server_iter;
			for (server_iter=service_iter->server_list->head; server_iter != NULL; server_iter = server_iter->next){
				strcpy(s, server_iter->serv_name);
				strcat(s, ":");
				sprintf(port, "%d", server_iter->port);
				strcat(s, port); 
				if( strcmp(s, substr)==0 ){
					break;
				}
			}
			if(server_iter == NULL){
				add_node(service_iter->server_list, substr);
			}	
		}
		free(port);
		free(s);
		str = strtok_r(NULL, "\n", &saveptr1);
	}
	char *s = (char*)malloc(1000*sizeof(char));
	char *port = (char*)malloc(1000*sizeof(char));
	while( (str=strtok_r(NULL, "\n", &saveptr1)) != NULL ){
		struct service *service_iter; /*iterator of service_list */
		for(service_iter = service_list->head; service_iter != NULL; service_iter = service_iter->next){
			if( strcmp(service_iter->function, str)==0 ){ /* found the function in service_list */
				struct node *server_iter;
				for(server_iter=service_iter->server_list->head; server_iter != NULL; server_iter=server_iter->next){
					strcpy(s, server_iter->serv_name);
					strcat(s, ":");
					sprintf(port, "%d", server_iter->port);
					strcat(s, port); 
					if(strcmp(s, server_name)==0){
						break;
					}
				}
				if(server_iter == NULL){
					add_node(service_iter->server_list, server_name); /* add the server to the server_list */
				}
				break;
			}
		}
		if(service_iter == NULL){ /* the service is not in service_list */
			add_service(service_list, str, server_name); /* add the service to service_list */
		}
	}
	free(s);
	free(port);
	printf("------ Current Data Structure ------\n");
	struct service *service_iter;
	for(service_iter = service_list->head; service_iter != NULL; service_iter = service_iter->next){
		printf("service: %s", service_iter->function);
		struct node *server_iter; 
		for (server_iter = service_iter->server_list->head; server_iter != NULL; server_iter = server_iter->next){
			printf(" -> %s:%d", server_iter->serv_name, server_iter->port);
		}
		printf("\n");
	}
	printf("------------------------------------\n\n");
	pthread_mutex_unlock(&handler_mutex); /* unlock */
	/* free */
	free(buff);
}
/* send hello msg to other servers 
	 protocal:	SERVER/HELLO\n 
	 						[server_name]:[port]\n
	 						[service #1]\n
	 						[service #2]\n
	 						...                     
	 						                          */
void send_server_info(int sockfd, char *own_server_name, char **service_table){
	char *s = (char*)malloc(SEND_MAX*sizeof(char));
	strcpy(s, "SERVER/HELLO\n");
	strcat(s, own_server_name);
	strcat(s, "\n");
	int i=0;
	for(i=0; i<own_service_table_size; i++){
		strcat(s, service_table[i]);
		strcat(s, "\n");
	}
	send(sockfd, s, strlen(s), 0);
	/* free */
	free(s);
}

/* determine whether the request is from server or client */
int is_server_request(char* buff){
		char *saveptr1;
		if (buff == NULL){
			return -1;
		}
	 	char temp_buff[strlen(buff)+1];
	 	strcpy(temp_buff, buff);
	 	char *str = strtok_r(temp_buff, "\n", &saveptr1);
	 	if (strcmp(str, "SERVER/HELLO")==0){
	 		return TRUE;
	 	}
	 	else if (strcmp(str, "CLIENT/REQUEST")==0){
	 		return FALSE;
	 	}
	 	else{
	 		error("Error, unknown protocol");
	 	}
	 	return TRUE;
}
/* thread: the request handler */
void handle_request(void *args){
	HandlerArgs *handlerArgs = (HandlerArgs*)args;
	char *recv_text = recv_once(handlerArgs->sockfd);
	int isServerRequest = is_server_request(recv_text);
	if(isServerRequest == TRUE){
		printf("This is a server request.\n");
		handle_server_request(handlerArgs->sockfd, recv_text, handlerArgs->own_service_table, handlerArgs->service_list, handlerArgs->own_server_name);
	}
	else if(isServerRequest == FALSE){
		printf("This is a client request.\n");
		handle_client_request(handlerArgs->sockfd, recv_text, handlerArgs->own_service_table, handlerArgs->service_list, handlerArgs->own_server_name);
	}
	else{
		printf("no infomation recived\n");
		printf("connection closed by client\n");
	}
	
	if(recv_text != NULL)
		free(recv_text);
	free(args);
	pthread_exit(NULL);
}

/* handle server requests */
void handle_server_request(int sockfd, char *recv_text, char **own_service_table, struct ServiceList *service_list, char *own_server_name){
	send_service_list_info(sockfd, own_server_name, own_service_table, service_list);
	char *saveptr1;
	char buff[strlen(recv_text)+1];
	strcpy(buff, recv_text);
	pthread_mutex_lock(&handler_mutex); /* lock */
	char *str = strtok_r(buff, "\n", &saveptr1);
	str = strtok_r(NULL, "\n", &saveptr1);
	char server_name[strlen(str)+1];
	strcpy(server_name, str);
	str = strtok_r(NULL, "\n", &saveptr1);
	while( str != NULL ){
		struct service *service_iter; /*iterator of service_list */
		for(service_iter = service_list->head; service_iter != NULL; service_iter = service_iter->next){
			if( strcmp(service_iter->function, str)==0 ){ /* found the function in service_list */
				add_node(service_iter->server_list, server_name); /* add the server to the server_list */
				break;
			}
		}
		if(service_iter == NULL){ /* the service is not in service_list */
			add_service(service_list, str, server_name); /* add the service to service_list */
		}
		str = strtok_r(NULL, "\n", &saveptr1);
	}
	pthread_mutex_unlock(&handler_mutex); /* unlock */
	printf("------ Current Data Structure ------\n");
	struct service *service_iter;
	for(service_iter = service_list->head; service_iter != NULL; service_iter = service_iter->next){
		printf("service: %s", service_iter->function);
		struct node *server_iter; 
		for (server_iter = service_iter->server_list->head; server_iter != NULL; server_iter = server_iter->next){
			printf(" -> %s:%d", server_iter->serv_name, server_iter->port);
		}
		printf("\n");
	}
	printf("------------------------------------\n\n");
	
	/* close connection */
	close(sockfd);
}

/* handle client request */
/* recv_text format: CLIENT/REQUEST\n
                     [function]\n
                     server #1
                     server #2
                     ...				      */
void handle_client_request(int sockfd, char *recv_text, char **own_service_table, struct ServiceList *service_list, char *own_server_name){
	int i=0;
	char *saveptr1, *saveptr2;
	char r[strlen(recv_text)+1];
	strcpy(r, recv_text);
	char *function = (char*)malloc(1000*sizeof(char)); /* parsed function from client */
	char *func_name = (char*)malloc(1000*sizeof(char)); /* parsed function name */
	int *param_list = (int*)malloc(1000*sizeof(int)); /* list of parameters from client */
	int param_len = 0; 
	/*********************************************/
	/* list of servers that the client has tried */
	/*********************************************/
	char **client_server_list = (char**)malloc(1000*sizeof(char*));
	int client_server_list_len = 0;
	for(i=0; i<1000; i++){
		client_server_list[i] = (char*)malloc(1000*sizeof(char));
		client_server_list[i][0] = '\0';
	}
	/*****************************/
	/* end of client_server_list */
	/*****************************/

	/*************************/
	/* Parsing the recv_text */
	/*************************/
	char *str = strtok_r(r, "\n", &saveptr1);
	str = strtok_r(NULL, "\n", &saveptr1);
	char *substr = strtok_r(str, " ", &saveptr2);
	strcpy(func_name, substr);
	strcpy(function, substr);
	i=0;
	while( (substr=strtok_r(NULL, " ", &saveptr2))!=NULL ){
		param_list[i] = (int)strtol(substr, NULL, 10);
		strcat(function, " int");
		++i;
	}
	param_len = i;
	//printf("function:%s | param_len:%d\n", function, param_len);
	i = 0;
	while ( (str=strtok_r(NULL, "\n", &saveptr1)) != NULL ){
		strcpy(client_server_list[i], str);
		++i;
	}
	client_server_list_len = i;
	/******************/
	/* end of parsing */
	/******************/
	
	send_result_to_client(sockfd, function, own_service_table, param_list, param_len, client_server_list, client_server_list_len, own_server_name, service_list, func_name);
	
	/* free */
	free(param_list);
	for (i=0; i<1000; i++){
		free(client_server_list[i]);
	}
	free(client_server_list);
}

/* send to result to client */
void send_result_to_client(int sockfd, char *function, char **own_service_table, int *param_list, int param_len, char **client_server_list, int client_server_list_len, char *own_server_name, struct ServiceList *service_list, char *func_name){
	int i=0;
	char *saveptr1;
	char *str;
	char *ns;
	int seed;
	int n;
	srand(time(NULL));
	n = rand() % 100;
	for(i=0; i<own_service_table_size; i++){
		char s[strlen(own_service_table[i])+1];
		strcpy(s, own_service_table[i]);
		str = strtok_r(s, " ", &saveptr1);
		if( strcmp(func_name, str)==0 ){
			if( strcmp(own_service_table[i], function)==0 ){
				break;
			}
			else{
				send_mismatch_message(sockfd, own_service_table[i]);
				return;
			}
		}
	}
	
	int service_num = i;
	ns = next_server(function, service_list, client_server_list, client_server_list_len);
	if( strcmp(own_service_table[i], "get_int int")==0 ){
		if (n < BUSY_PERCENT){
			if(ns == NULL){
				printf("Server is busy, and the client has tried all the possible servers, sending out messsage.\n");
				send_out_message(sockfd);
			}
			else{
				send_busy_message(sockfd, function, ns); 
			}
		}
		else{
			int r = get_int(param_list[0]);
			send_result_message(sockfd, function, r);
		}			
	}
	else if( strcmp(own_service_table[i], "add int int")==0 ){
		if (n < BUSY_PERCENT){
			if(ns == NULL){
				printf("Server is busy, and the client has tried all the possible servers, sending out messsage.\n");
				send_out_message(sockfd);
			}
			else{
				send_busy_message(sockfd, function, ns); 
			}
		}
		else{
			int r = add(param_list[0], param_list[1]);
			send_result_message(sockfd, function, r);
		}			
	}
	else if( strcmp(own_service_table[i], "sub int int")==0 ){
		if (n < BUSY_PERCENT){
			if(ns == NULL){
				printf("Server is busy, and the client has tried all the possible servers, sending out messsage.\n");
				send_out_message(sockfd);
			}
			else{
				send_busy_message(sockfd, function, ns); 
			}
		}
		else{
			int r = sub(param_list[0], param_list[1]);
			send_result_message(sockfd, function, r);
		}			
	}
	else if( strcmp(own_service_table[i], "mul int int")==0 ){
		if (n < BUSY_PERCENT){
			if(ns == NULL){
				printf("Server is busy, and the client has tried all the possible servers, sending out messsage.\n");
				send_out_message(sockfd);
			}
			else{
				send_busy_message(sockfd, function, ns); 
			}
		}
		else{
			int r = mul(param_list[0], param_list[1]);
			send_result_message(sockfd, function, r);
		}			
	}
	else if( strcmp(own_service_table[i], "a int int")==0 ){
		if (n < BUSY_PERCENT){
			if(ns == NULL){
				printf("Server is busy, and the client has tried all the possible servers, sending out messsage.\n");
				send_out_message(sockfd);
			}
			else{
				send_busy_message(sockfd, function, ns); 
			}
		}
		else{
			int r = a(param_list[0], param_list[1]);
			send_result_message(sockfd, function, r);
		}			
	}
	else if( strcmp(own_service_table[i], "b int int")==0 ){
		if (n < BUSY_PERCENT){
			if(ns == NULL){
				printf("Server is busy, and the client has tried all the possible servers, sending out messsage.\n");
				send_out_message(sockfd);
			}
			else{
				send_busy_message(sockfd, function, ns); 
			}
		}
		else{
			int r = b(param_list[0], param_list[1]);
			send_result_message(sockfd, function, r);
		}			
	}
	else if( strcmp(own_service_table[i], "c int int")==0 ){
		if (n < BUSY_PERCENT){
			if(ns == NULL){
				printf("Server is busy, and the client has tried all the possible servers, sending out messsage.\n");
				send_out_message(sockfd);
			}
			else{
				send_busy_message(sockfd, function, ns); 
			}
		}
		else{
			int r = c(param_list[0], param_list[1]);
			send_result_message(sockfd, function, r);
		}			
	}
	else if( strcmp(own_service_table[i], "d int int")==0 ){
		if (n < BUSY_PERCENT){
			if(ns == NULL){
				printf("Server is busy, and the client has tried all the possible servers, sending out messsage.\n");
				send_out_message(sockfd);
			}
			else{
				send_busy_message(sockfd, function, ns); 
			}
		}
		else{
			int r = d(param_list[0], param_list[1]);
			send_result_message(sockfd, function, r);
		}			
	}
	else if( strcmp(own_service_table[i], "e int int")==0 ){
		if (n < BUSY_PERCENT){
			if(ns == NULL){
				printf("Server is busy, and the client has tried all the possible servers, sending out messsage.\n");
				send_out_message(sockfd);
			}
			else{
				send_busy_message(sockfd, function, ns); 
			}
		}
		else{
			int r = e(param_list[0], param_list[1]);
			send_result_message(sockfd, function, r);
		}			
	}
	else if( strcmp(own_service_table[i], "f int int")==0 ){
		if (n < BUSY_PERCENT){
			if(ns == NULL){
				printf("Server is busy, and the client has tried all the possible servers, sending out messsage.\n");
				send_out_message(sockfd);
			}
			else{
				send_busy_message(sockfd, function, ns); 
			}
		}
		else{
			int r = f(param_list[0], param_list[1]);
			send_result_message(sockfd, function, r);
		}			
	}
	else{
		if(ns == NULL){
			printf("Function <%s> is unknown to the server, and no other server is able to handle this function, sending out message.\n", function);
			send_out_message(sockfd);
		}
		else{
			send_unknown_message(sockfd, function, ns);
		}
	}
	/* free */
	if(ns != NULL)
		free(ns);
}

/* return the next server to connect in case of unknown/busy */
char *next_server(char *function, struct ServiceList *service_list, char **client_server_list, int client_server_list_len){
	int i;
	struct service *service_iter;
	for(service_iter = service_list->head; service_iter != NULL; service_iter = service_iter->next){
		if( strcmp(service_iter->function, function)==0 ){
			break;
		}
	}
	if(service_iter==NULL){
		return NULL;
	}
	struct node *server_iter;
	char *server_name = (char*) malloc(1000*sizeof(char));
	for(server_iter = service_iter->server_list->head; server_iter != NULL; server_iter = server_iter->next){
		get_full_server_name(server_name, server_iter->serv_name, server_iter->port);
		int match = FALSE;
		for(i=0; i < client_server_list_len; i++){	
			if(strcmp(client_server_list[i], server_name)==0 ){
				match = TRUE;
				break;
			}
		}
		if(match == FALSE){
			break;
		}
	}
	if (server_iter == NULL){
		free(server_name);	
		return NULL;
	}
	else{
		return server_name;
	}
} 
/* send the SERVER/UNKONW message */
/* format: SERVER/UNKONW\n
					 [function]\n
           [server_name]\n */
void send_unknown_message(int sockfd, char *function, char *server_name){
	printf("Function <%s> is unknown to server, recommending <%s> to the client.\n", function, server_name);
	char s[strlen("SERVER/UNKNOWN\n")+strlen(function)+strlen(server_name) +10];
	strcpy(s, "SERVER/UNKNOWN\n");
	strcat(s, function);
	strcat(s, "\n");
	strcat(s, server_name);
	strcat(s, "\n");
	send(sockfd, s, strlen(s), 0);
}

/* send the SERVER/BUSY message */
/* format: SERVER/UNKONW\n
					 [function]\n
           [server_name]\n */
void send_busy_message(int sockfd, char *function, char *server_name){
	printf("Server is busy, recommending <%s> to the client.\n", server_name);
	char s[strlen("SERVER/BUSY\n")+strlen(function)+strlen(server_name) +10];
	strcpy(s, "SERVER/BUSY\n");
	strcat(s, function);
	strcat(s, "\n");
	strcat(s, server_name);
	strcat(s, "\n");
	send(sockfd, s, strlen(s), 0);
}
/* send the SERVER/MISMATCH message */
/* format: SERVER/MISMATCH\n
					 [function]\n */
void send_mismatch_message(int sockfd, char *function){
	printf("Function parameter mismatch, needs to be <%s>.\n", function);
	char s[strlen(function)+strlen("SERVER/MISMATCH\n") + 10];
	strcpy(s, "SERVER/MISMATCH\n");
	strcat(s, function);
	strcat(s, "\n");
	send(sockfd, s, strlen(s), 0);
}

/* send result to client */
/* format: SERVER/RESULT\n
				   [function]\n
				   [result(string)]\n
				                           */
void send_result_message(int sockfd, char *function, int result){
	printf("The result is <%d>, sending to client.\n", result);
	char *r = (char*)malloc(1000*sizeof(char));
	sprintf(r, "%d", result);
	char s[strlen("SERVER/RESULT\n")+strlen(function)+strlen(r)+10];
	strcpy(s, "SERVER/RESULT\n");
	strcat(s, function);
	strcat(s, "\n");
	strcat(s, r);
	strcat(s, "\n");
	free(r);
	send(sockfd, s, strlen(s), 0);
}
/* send message when all servers are busy */
void send_out_message(int sockfd){
	send(sockfd, "SERVER/OUT\n", strlen("SERVER/OUT\n"), 0);
}
/* get full server name */
void get_full_server_name(char *full_server_name, char *server_name, int port_no){
	strcpy(full_server_name, server_name);
	char *port = (char*)malloc(1000*sizeof(char));
	sprintf(port, "%d", port_no);
	strcat(full_server_name, ":");
	strcat(full_server_name, port);
	free(port);
}
/* send service list information to the other server */
/* format: [server_name]:[port]
					 [service]&[serv_name]:[port]&[serv_name]:[port]...
					 ...
					 TABLE\n
					 [service #1]\n
					 [service #2]\n
					 ....
					                                               */
void send_service_list_info(int sockfd, char *own_server_name, char **own_service_table, struct ServiceList *service_list){
	char *s = (char*)malloc(SEND_MAX*sizeof(char));
	strcpy(s, own_server_name);
	strcat(s, "\n");
	struct service *service_iter;
	for(service_iter = service_list->head; service_iter != NULL; service_iter = service_iter->next){
		strcat(s, service_iter->function);
		struct node *server_iter;
		for(server_iter = service_iter->server_list->head; server_iter != NULL; server_iter = server_iter->next){
			strcat(s, "&");
			strcat(s, server_iter->serv_name);
			strcat(s, ":");
			char *port = (char*)malloc(100*sizeof(char));
			sprintf(port, "%d", server_iter->port);
			strcat(s, port);
			free(port);
		}
		strcat(s, "\n");
	}
	strcat(s, "TABLE\n");
	int i = 0;
	for(i=0; i<own_service_table_size; i++){
		strcat(s, own_service_table[i]);
		strcat(s, "\n");
	}
	send(sockfd, s, strlen(s), 0);
	
	free(own_server_name);
	free(s);
}
/* receive, corresponding to one send only */
char* recv_once(int sockfd){
	int buff_size = 1000;
	char buff[buff_size+1];
	char *full_text = (char*)malloc(SEND_MAX*sizeof(char));
	full_text[0] = '\0';
	int nbytes;
	while ( (nbytes = recv(sockfd, buff, buff_size, 0)) == buff_size){
		buff[nbytes] = '\0';
		strcat(full_text, buff);
	}
	if(nbytes != 0){
		buff[nbytes] = '\0';
		strcat(full_text, buff);
	}
	if(full_text[0] == '\0'){
		free(full_text);
		full_text = NULL;
	}
	return full_text;
}

/* utility: convert string to lower case characters */
void to_l_case(char *s){
	int i;
	for(i=0; s[i]!='\0'; i++){
		s[i] = tolower(s[i]);
	}
}

/* check the arguments of main */
void check_arguments(int argc, char **argv, int *new_argc, char ***new_argv, char **service_table, char **own_service_table){
	own_service_table_size = 0;
	int i=0;
	int j=0;
	char *saveptr1;
	char *func_name_list[SERVICE_TABLE_SIZE];
	for(i=0; i<SERVICE_TABLE_SIZE; i++){
		func_name_list[i] = (char*)malloc(500*sizeof(char));
		char s[strlen(service_table[i])+1];
		strcpy(s, service_table[i]);
		char *str = strtok_r(s, " ", &saveptr1);
		strcpy(func_name_list[i], str);
	}
	#define free_func_name_list()\
		for(i=0; i<SERVICE_TABLE_SIZE; i++){\
			free(func_name_list[i]);\
		}
	if(argc < 2){
		*new_argc = argc;
		*new_argv = argv;
		for(i=0; i<SERVICE_TABLE_SIZE; i++){
			strcpy(own_service_table[i], service_table[i]);
		}
		own_service_table_size = SERVICE_TABLE_SIZE;
		free_func_name_list();
		return;
	}
	if( strcmp(argv[1], "-s")==0 ){
		*new_argc = argc - 1;
		*new_argv = argv+1;
		for(i=0; i<SERVICE_TABLE_SIZE; i++){
			strcpy(own_service_table[i], service_table[i]);
		}
		own_service_table_size = SERVICE_TABLE_SIZE;
		free_func_name_list();
		return;
	}
	if ( strcmp(argv[1], "-s")!=0 && strcmp(argv[1], "-function")!=0 ){
		printf("The second argument must be -s or -function, please retry.\n");
		exit(1);
	}
	for(i=2; i<argc && strcmp(argv[i], "-s")!=0; i++){
		int match = FALSE;
		for(j=0; j<SERVICE_TABLE_SIZE; j++){
			if( strcmp(argv[i], func_name_list[j])==0 ){
				match = TRUE;
				strcpy(own_service_table[own_service_table_size], service_table[j]);
				++own_service_table_size;
				break;
			}
		}
		if(match == FALSE){
			printf("Function <%s> isn't supported by the server, please retry.\n", argv[i]);
			exit(1);
		}
	}
	if(own_service_table_size == 0){
		printf("Function list can not be empty, please retry.\n");
		exit(1);
	}
	*new_argc = argc - i;
	*new_argv = argv+i; 
	//printf("new_argv: %s\n", (*new_argv)[0]);
	//printf("new_argc: %d\n", *new_argc);
	free_func_name_list();
}
/**************************************/
/* Services that this server provides */
/**************************************/

/* service# 0 */
int get_int(int a){
	return a;
}

/* service# 1 */
int add(int a, int b){
	return a+b;
}

/* service# 2 */
int sub(int a, int b){
	return a-b;
}

/* service# 3 */
int mul(int a, int b){
	return (float)a *b;
}
/* service# 4 */
int a(int a, int b){
	return a+b;
}
/* service# 5 */							
int b(int a, int b){
	return a+b;
}
/* service# 6 */						
int c(int a, int b){
	return a+b;
}
/* service# 7 */						
int d(int a, int b){
	return a+b;
}
/* service# 8 */						
int e(int a, int b){
	return a+b;
}
/* service# 9 */						
int f(int a, int b){
	return a+b;
}						
/*******************/
/* End of Services */
/*******************/
