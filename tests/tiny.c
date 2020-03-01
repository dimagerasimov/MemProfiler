#include <stdlib.h>

int main(int argc, char *argv[]) {
    int *a = malloc(8 * sizeof(char));

    char *b = calloc(8, sizeof(char));
 
    void *d = realloc(a, 0);

    return 0;
}
