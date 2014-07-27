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
#define FALSE 0
#define TRUE 1
#define SEND_MAX 100000

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

/* getline */
char *getl(int len){
	char temp[len];
	char c[2];
	temp[0] = '\0';
	int i =0;
	for(i=0; i<len; i++){
		c[0] = getchar();
		if(c[0] != '\n'){
			c[1] = '\0';
			strcat(temp, c);
		}
		else{
			break;
		}
	}
	char *s = (char*)malloc( (strlen(temp)+1)*sizeof(char));
	strcpy(s, temp);
	return s;
}

int main(int argc, char** argv){
 char *line = getl(1000);
 printf("%s\n", line);
 remove_space(line);
 printf("%s\n", line);
 return 0;
}
