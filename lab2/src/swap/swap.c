#include "swap.h"

void Swap(char *left, char *right)
{
        char third = *left;
        *left = *right;
        *right = third;
}
