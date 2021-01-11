#include <stdio.h>

#include <pv/pv.h>

int main(int argc, char** argv) {
	int ret = pv_init(argc, argv);
	printf("pv_init(): %d\n", ret);

	return 0;
}
