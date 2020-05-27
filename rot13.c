
//
// rot13.c
//
// Copyright (c) 2013 Stephen Mathieson
// MIT licensed
//

#include "rot13.h"

/*
 * Magic number.
 */

#define ROT13_MODIFIER 13

/*
 * Rotate the given `str` inline.
 */

char *rot13(char *str) {
  for (int i = 0; '\0' != str[i]; i++) {
    char c = *(str + i);
    if (('a' <= c && 'n' > c) || ('A' <= c && 'N' > c)) {
      // a-m
      *(str + i) += ROT13_MODIFIER;
    } else if (('n' <= c && 'z' >= c) || ('N' <= c && 'Z' >= c)) {
      // n-z
      *(str + i) -= ROT13_MODIFIER;
    }
  }
  return str;
}
