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
#define FALSE 0
#define TRUE 1
#define SEND_MAX 100000
#define SERVER_LIST_SIZE 10000

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

int main(int argc, char **argv){
char *line = getl(1000);
printf("%s\n", line);
return 0;
}
