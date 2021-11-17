#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

int main(int argc, char** argv){

    FILE* fo;

    //Open file and handle error
    if((fo = fopen(argv[1], "r")) == NULL){
        exit(1);
        printf("Error opening file\n");
    }
    else printf("File opened\n");

    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    
    //Read file
    while ((read = getline(&line, &len, fo)) != -1) {
        printf("Line length %zu\n", read);
        printf("0x%x\n", line[0]);
    }

    //Close file
    fclose(fo);
    if (line) free(line);
    exit(EXIT_SUCCESS);

}
