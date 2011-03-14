#include "tests/lib.h"
static const char prog[] = "[hamster]test"; /* The program to start. */

int main(void)
{
	int handle = syscall_open(prog);
	printf("Handle: %i\n", handle);
	handle = syscall_open(prog);
	printf("Handle: %i\n", handle);
	syscall_halt();
	return 0;
}
