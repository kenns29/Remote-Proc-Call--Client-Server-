#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

char *getl(int);
void to_l_case(char*);
int main(int argc, int argv){
	char *s = getl(1000);
	to_l_case(s);
	printf("%s\n", s);
	return 0;
}

void to_l_case(char *s){
	int i;
	for(i=0; s[i]!='\0'; i++){
		s[i] = tolower(s[i]);
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
