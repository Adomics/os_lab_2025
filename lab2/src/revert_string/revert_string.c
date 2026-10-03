#include "revert_string.h"
#include <stdlib.h>
#include <string.h>

void RevertString(char *str)
{
        size_t len = strlen(str);
        char *result = malloc(len + 1);
        for (size_t i = 0; i < len; i++) {
            result[i] = str[len - 1 - i];
        }
        result[len] = '\0';
        strcpy(str, result);
        free(result);
}
