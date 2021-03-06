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
#include <limits.h>
#include <math.h>
#include <ctype.h>
#define FALSE 0
#define TRUE 1
#define SEND_MAX 100000
#define SERVER_LIST_SIZE 10000

#define SERVER_RESULT 0
#define SERVER_UNKNOWN 1
#define SERVER_BUSY 2
#define SERVER_MISMATCH 3
#define SERVER_OUT 4

void error(const char *s){
	fprintf(stderr, "%s\nerrno: %s\n", s, strerror(errno));
	exit(1);
}
int parse_server_name(char*, char*, char*);
char *parse_function(char*);
int isNumeric(char*);
int isInteger(char*);
void send_request(int, char*, char**, int);
char *getl(int);
char *recv_once(int);
int process_recv_text(char*, char**, int*, char*);
char *try_other_server(int next_sockfd, char *recv_text, char *function, char **server_list, int sl_size, char*);
void remove_space(char*);
void get_full_server_name(char*, char*, int);
void to_l_case(char*);
/* main */
int main(int argc, char **argv){
	int i;
	char *current_server_name = (char*)malloc(1000*sizeof(char));
	/********************************************************************/
	/* server_list that maintains a list of servers that has been tried */
	/********************************************************************/
	char **server_list = (char**)malloc(SERVER_LIST_SIZE*sizeof(char*));
	for(i=0; i<10000; i++){
		server_list[i] = (char*)malloc(10000*sizeof(char));
		server_list[i][0] = '\0';
	}
	int server_list_size = 0;
	/**********************/
	/* end of server_list */
	/**********************/

	/* Establish the Connection */
	char *server_name = (char*)malloc(1000*sizeof(char));
	char *port = (char*)malloc(1000*sizeof(char));
	while(TRUE){	
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		struct addrinfo hints;
		struct addrinfo *servinfo;
		memset(&hints, '0', sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM; 
		hints.ai_flags = AI_PASSIVE;
		hints.ai_protocol = IPPROTO_TCP;
		while(TRUE){
			printf("Please Enter the Name of Server to Connect:\n");
			char *full_server_name = getl(1000);
			remove_space(full_server_name);
			to_l_case(full_server_name);
			/* parse_server_name */
			if ( parse_server_name(full_server_name, server_name, port)==FALSE ){
				printf("Please Re-Enter the Address.\n");
				continue;
			}
		  free(full_server_name);
			/* getaddrinfo */
			int status = getaddrinfo(server_name, port, &hints, &servinfo); 
			if (status != 0){
				printf("%s\n", gai_strerror(status));
				error("error in getaddrinfo");
			}
			else{
				printf("connecting to a server\n");
			}
			if( connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) != 0 ){
				printf("Error in connecting to the remove address, please re-enter the address:\n");
				continue;
			}
			
			/******************************/
  		/* get the name of the server */
  		/******************************/
  		
  		char host_info[SEND_MAX];
  		char service_info[SEND_MAX];
  		getnameinfo(servinfo->ai_addr, servinfo->ai_addrlen, host_info, sizeof host_info, service_info, sizeof service_info, 0);
  		to_l_case(host_info);
  		int port_no = (int)strtol(port, NULL, 10);
  		get_full_server_name(current_server_name, host_info, port_no);
  		printf("Current Server Name: %s\n", current_server_name);
  		/**********************/
  		/* end of getnameinfo */
  		/**********************/
			freeaddrinfo(servinfo);
			printf("connection established\n");
			break;
		}
		
		char *function;
	printf("\nPlease Enter the Function to Execute.\n(Note that the Function must be the form [func name]([param1], [param2]....),\nonly integers are allowed, and parameters can only be constant.\nex. a(1, 2, 8) ):\n");
		while(TRUE){
			char *line = getl(1000);
			function = parse_function(line);
			free(line);
			if(function != NULL){
				break;
			}
			else{
				printf("Invalid function format, please try again:\n");
			}
		}
		
		send_request(sockfd, function, server_list, server_list_size);
		char *recv_text = recv_once(sockfd);
		if(recv_text==NULL){
			printf("Connection closed by remote server.\n");
			return 0;
		}
		
		int status = process_recv_text(recv_text, server_list, &server_list_size, current_server_name);
		if (status != SERVER_UNKNOWN && status != SERVER_BUSY){
			/* clear up for the next round */
			free(function);
			close(sockfd);	
			for(i=0; i<server_list_size; i++){
				server_list[i][0] = '\0';
			}
			server_list_size = 0;
			printf("---------- Next Round -----------\n");
			continue;
		}
		 
		int next_sockfd; /* = socket(AF_INET, SOCK_STREAM, 0);*/
		char *pre_recv_text = recv_text;
		char *next_recv_text;
		while(TRUE){
			next_recv_text = try_other_server(next_sockfd, pre_recv_text, function, server_list, server_list_size, current_server_name);
			free(pre_recv_text);
			if(next_recv_text==NULL){
				printf("Connection closed by remote server.\n");
				break;
			}
			int status = process_recv_text(next_recv_text, server_list, &server_list_size, current_server_name);
			if(status != SERVER_UNKNOWN && status != SERVER_BUSY){
				break;
			}
			pre_recv_text = next_recv_text;
		}
		
		/* clear up for the next round */
		if(next_recv_text != NULL)
			free(next_recv_text);
		free(function);
		close(sockfd);	
		for(i=0; i<server_list_size; i++){
			server_list[i][0] = '\0';
		}
		server_list_size = 0;
		printf("---------- Next Round -----------\n");
	}	
return 0;
}
	
/* parse the server name */
/* param: s must be the form [server_name]:[port_number]
					n is the [server_name] (return value)
					port is the [port_number] in integer (return value) */
/* return: TRUE or FALSE */
int parse_server_name(char *s, char *n, char *port){
	char temp[strlen(s)+1];
	strcpy(temp, s);
	char *str = strtok(temp, ":");
	strcpy(n, str);
	str = strtok(NULL, ":");
	if (str == NULL){
		printf("Error:addresses must be the form [server_name]:[port_number].");
		return FALSE;
	}
	strcpy(port, str);
	long port_no = strtol(port, NULL, 10);
	if( port_no == 0L ){
		printf("Invalid port number.");
		return FALSE;
	}
	else if( port_no == LONG_MAX || port_no == LONG_MIN ){
		printf("Port number entered is too large.");
		return FALSE;
	}
	return TRUE;
}

/* check whether the function is in valid form and convert it the form server accepts */
/* input valid form: [return type] [function name]([param1], [param2]..) */
/* server acceptable form: [return type] [function name] [param1] [param2]... */
/* Return: NULL when invalid form detected or the parsed function when successs */
char *parse_function(char *buff){
	char t[strlen(buff)+1];
	char temp[strlen(buff)+1];
	strcpy(temp, buff);

	char *str = strtok(temp, " ()");
	if(str == NULL){
		return NULL;
	}
	//strcat(t, " ");
	strcpy(t, str);
	while( (str=strtok(NULL, " ,()")) != NULL){
		if(isInteger(str)){
			strcat(t, " ");
			strcat(t, str);
		}
		else{
			return NULL;
		}
	}
	char *s = (char*)malloc( (strlen(t)+1)*sizeof(char) );
	strcpy(s, t);
	return s;
}

/* check whether the string can be converted to number */
/* return TRUE or FALSE */
int isNumeric(char *s){
	long l = strtol(s, NULL, 10);
	if( l != 0L && l != LONG_MAX && l != LONG_MIN ){
		return TRUE;
	}
	float n = strtof(s, NULL);
	if(n==0.0F)
		return FALSE;
	else if(n == HUGE_VALF){
		return FALSE;
	}
	return TRUE;
}

/* check whether the string can be converted to integer */
/* return TRUE or FALSE */
int isInteger(char *s){
	long l = strtol(s, NULL, 10);
	if( l == 0L || l == LONG_MAX || l == LONG_MIN ){
		return FALSE;
	}
	else{
		return TRUE;
	}
}
/* getline */
char *getl(int len){
	char temp[len];
	char c[2];
	temp[0] = '\0';
	int i =0;
	while(i<len){
		c[0] = getchar();
		if(c[0] == '\n'){
			break;
		}
		else if(c[0] == '\b'){
			if(i==0){
				continue;
			}
			else{
				--i;
				temp[i] = '\0';
			}
		}
		else{
			++i;
			c[1] = '\0';
			strcat(temp, c);
		}
	}
	char *s = (char*)malloc( (strlen(temp)+1)*sizeof(char));
	strcpy(s, temp);
	return s;
}

/* send request to remote server */
void send_request(int sockfd, char *function, char **server_list, int sl_size){
	int i = 0;
	char s[strlen(function)+strlen("CLIENT/REQUEST\n")+2];
	strcpy(s, "CLIENT/REQUEST\n");
	strcat(s, function);
	strcat(s, "\n");
	for(i=0; i<sl_size; i++){
		strcat(s, server_list[i]);
		strcat(s, "\n");
	}
	send(sockfd, s, strlen(s), 0);
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

int process_recv_text(char *recv_text, char **server_list, int *sl_size, char *current_server_name){
	char s[strlen(recv_text)+1];
	strcpy(s, recv_text);
	char *saveptr1;
	char *str;
	str = strtok_r(s, "\n", &saveptr1);
	if( strcmp(str, "SERVER/UNKNOWN")==0 ){
		str = strtok_r(NULL, "\n", &saveptr1);
		str = strtok_r(NULL, "\n", &saveptr1);
		
		strcpy(server_list[*sl_size], current_server_name);
		++(*sl_size); 
		printf("Service Unknown to the Current Server. Trying the Recommanded Server <%s>.\n", str);
		return SERVER_UNKNOWN;
	}
	else if( strcmp(str, "SERVER/BUSY")==0 ){
		str = strtok_r(NULL, "\n", &saveptr1);
		str = strtok_r(NULL, "\n", &saveptr1);
		strcpy(server_list[*sl_size], current_server_name);
		++(*sl_size);
		printf("Current Server is busy. Trying the Recommanded Server <%s>.\n", str); 
		return SERVER_BUSY;
	}
	else if( strcmp(str, "SERVER/MISMATCH")==0 ){
		str = strtok_r(NULL, "\n", &saveptr1);
		printf("Incorrect number of parameters for this function.\n");
		printf("Should be: %s\n", str);
		return SERVER_MISMATCH;
	}
	else if( strcmp(str, "SERVER/RESULT")==0 ){
		str = strtok_r(NULL, "\n", &saveptr1);
		str = strtok_r(NULL, "\n", &saveptr1);
		printf("***********\n");
		printf("RESULT: %s\n", str);
		printf("***********\n");
		return SERVER_RESULT;
	}
	else if( strcmp(str, "SERVER/OUT")==0 ){
		printf("No server was able to execute the function, either they were all busy or none of them recognize the function, please try again.\n");
		return SERVER_OUT;
	}
	else{
		return -1;
	}
}

char *try_other_server(int next_sockfd, char *pre_recv_text, char *function, char **server_list, int sl_size, char *current_server_name){
	next_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct addrinfo hints;
	struct addrinfo *servinfo;
	memset(&hints, '0', sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;
	
	char *server_name = (char*)malloc(1000*sizeof(char));
	char *port = (char*)malloc(1000*sizeof(char));
	
	char s[strlen(pre_recv_text)+1];
	strcpy(s, pre_recv_text);
	char *saveptr1;
	char *str = strtok_r(s, "\n", &saveptr1);
	str = strtok_r(NULL, "\n", &saveptr1);
	str = strtok_r(NULL, "\n", &saveptr1);
	
	char full_server_name[strlen(str)+1];
	strcpy(full_server_name, str);
	strcpy(current_server_name, str);
		/* parse_server_name */
	if ( parse_server_name(full_server_name, server_name, port)==FALSE ){
		error("incorrect format for full_server_name.");
	}
	  
	if ( strcmp(server_name, "localhost")==0 ){
		strcpy(server_name, "127.0.0.1");
	}
	/* getaddrinfo */
	int status = getaddrinfo(server_name, port, &hints, &servinfo); 
	if (status != 0){
		printf("%s\n", gai_strerror(status));
		error("error in getaddrinfo");
	}
	else{
		printf("connecting to a server.\n");
	}
	if( connect(next_sockfd, servinfo->ai_addr, servinfo->ai_addrlen) != 0 ){
		error("Error in connecting to the remote address");
	}
	freeaddrinfo(servinfo);
	printf("connection established.\n");
	send_request(next_sockfd, function, server_list, sl_size);
	char *next_recv_text = recv_once(next_sockfd);
	close(next_sockfd);
	return next_recv_text;
} 

/* utility: removes space of a string */
void remove_space(char *s){
	char *saveptr1;
	char *str;
	char t[strlen(s)+1];
	strcpy(t, s);
	str = strtok_r(s, " ", &saveptr1);
	strcpy(s, str);
	while( (str=strtok_r(NULL, " ", &saveptr1))!=NULL ){
		strcat(s, str);
	}
}

/* utility: convert string to lower case characters */
void to_l_case(char *s){
	int i;
	for(i=0; s[i]!='\0'; i++){
		s[i] = tolower(s[i]);
	}
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





