#include <string.h>

unsigned char bdstr[] = "blah                            ";

char *StringPadRight(char *string, int padded_len, char *pad) {
    int len = (int) strlen(string);
    if (len >= padded_len) {
        return string;
    }
    int i;
    for (i = 0; i < padded_len - len; i++) {
        strcat(string, pad);
    }
    return string;
}

int main() {
	char *bdenc1 = "blah";
	char bdenc[33];
	strcpy(bdenc, bdenc1);
//	printf(bdenc);
//	char bdencc[32] = *bdenc;
//	printf(bdenc);
	StringPadRight(bdenc, 32, " ");
	if (strcmp(bdenc, bdstr) != 0) {
		printf("false");
	} else {
		printf("true");
	}
	printf("%s", bdstr);
	free(bdenc);
	return 0;
}
