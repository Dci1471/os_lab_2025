#include "revert_string.h"

void RevertString(char *str)
{
    char *end = str;
    while (*end) {
        ++end;
    }
    --end;                            
    char *start = str;
    while (start < end) {
        char tmp = *start;
        *start = *end;
        *end = tmp;
        ++start;
        --end;h
    }
}

