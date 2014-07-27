#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv){

char *s = (char*)malloc(1000*sizeof(char));
strcpy(s, "1 2 3 4 5\n6 7 8 9 10\n11 12 13 14 15\n16");
char *str, *substr;
char *saveptr1, *saveptr2;

str = strtok_r(s, "\n", &saveptr1);
while (str != NULL){
	printf("str: %s\n", str);
	substr = strtok_r(str, " ", &saveptr2);
	while(substr != NULL){
		printf("substr: %s\n", substr);
		substr = strtok_r(NULL, " ", &saveptr2);
	}
	str = strtok_r(NULL, "\n", &saveptr1);
}

return 0;
}
