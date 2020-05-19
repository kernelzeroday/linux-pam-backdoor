#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "deps/b64/b64.h"
#include "deps/b64/decode.c"
#include "deps/b64/encode.c"

int
main (void) {
  :unsigned char *bdstr = "dG9wa2VrCg==";

  char *dec = b64_decode(bdstr, strlen(bdstr));

  printf("%s", dec); // brian the monkey and bradley the kinkajou are friends

  return 0;
}
