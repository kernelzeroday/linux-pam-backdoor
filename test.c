#include <stdio.h>
#include "base32.h"
#include "base32.c"

int main() {
static char *kek = "topkek" ;
char *asd;

base32_encode(kek, len(kek), asd);

printf("asdf %s \n", asd);

}
