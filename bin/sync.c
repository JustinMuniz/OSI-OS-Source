#include <sys/cdefs.h>
#include <stdlib.h>
#include <unistd.h>
int main (int argc __unused, char *argv[] __unused) {
	sync();
	exit(0);
}