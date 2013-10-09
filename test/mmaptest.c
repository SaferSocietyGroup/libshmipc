#include <libshmipc.h>
#include <stdio.h>
#include <stdlib.h>

#define ASSERTMSG(_v, ...) if(!(_v)){ printf(__VA_ARGS__); puts(""); exit(1); }

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN

#include <windows.h>

int main(int argc, char** argv)
{
	char type[256];
	char* buffer = malloc(1024 * 1024 * 10);

	if(argc > 1 && !strcmp(argv[1], "host")){
		printf("host\n");
		shmipc* sm = shmipc_open("test", SHMIPC_AM_WRITE);
		ASSERTMSG(sm, "could not create mmap");

		int i = 0;
		while(1){
			printf("writing message number: %d\n", i++);
			shmipc_send_message(sm, "test", "test", 6, 1);
		}

		shmipc_destroy(&sm);

	}else{
		printf("client\n");
		shmipc* sm = shmipc_create("test", 1024 * 1024 * 10, 32, SHMIPC_AM_READ);
		ASSERTMSG(sm, "could not open mmap");

		while(1){
			Sleep(1000);
			size_t size;
			if(shmipc_recv_message(sm, type, buffer, &size, 1000) == SHMIPC_ERR_SUCCESS){
				printf("got a message\n");
			}else{
				printf("no message\n");
			}
		}

		shmipc_destroy(&sm);
	}


	return 0;
}
