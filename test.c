#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

int main(int argc, char **argv){

int list[100];

int i;
for (i=0; i<100; i++){
	list[i] = i;
}
int list_len = i;

int *new_list = list+50;
int new_list_len = 100-50;

for(i=0; i<new_list_len; i++){
	printf("%d\n", new_list[i]);
}

return 0;
}

