#include <stdio.h>
#include <unistd.h>

int main()
{
    write(1, "This goes to stdout\n", 20);
    write(2, "This goes to stderr\n", 20);
    return 0;
}
