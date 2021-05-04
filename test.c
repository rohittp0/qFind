#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    int a=9999;
    char buffer[21];
    sprintf(buffer, " %9.3f TiB ", 9999.999);
    sprintf(buffer," %9.0f TiB ", 1234.33);
    printf("Binary value = %s\n", buffer);

    return 0;
}