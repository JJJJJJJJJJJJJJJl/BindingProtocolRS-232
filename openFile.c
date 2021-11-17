#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

int main(int argc, char** argv){

    FILE* fo;

    if((fo = fopen(argv[1], "r")) == NULL){
        exit(1);
        printf("Erro ao ler ficheiro\n");
    }
    else printf("Fichero lido\n");

    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    
    while ((read = getline(&line, &len, fo)) != -1) {
        printf("Retrieved line of length %zu:\n", read);
        printf("%s\n", line);
    }

    fclose(fo);
    if (line) free(line);
    exit(EXIT_SUCCESS);

}
