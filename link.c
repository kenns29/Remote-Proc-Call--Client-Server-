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
#define FALSE 0
#define TRUE 1
#define SEND_MAX 100000

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
/* parameter: fun must be the form of [proc] [pram] [pram] ... */
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

/* parameter: fun must be the form of [proc] [pram] [pram] ... */
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

int main(int argc, char **argv){
	struct ServiceList* s = new_empty_ServiceList();
	add_empty_service(s, "int f 1 2 3");
	add_node(s->head->server_list, "locahost:1000");
	printf("%s, %s, %d\n",s->head->function, s->head->server_list->head->serv_name, s->head->server_list->head->port);
}
