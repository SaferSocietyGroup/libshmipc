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
		shmipc* sm;

		printf("host\n");

		shmipc_error err = shmipc_open("test", SHMIPC_AM_WRITE, &sm);
		ASSERTMSG(err == SHMIPC_ERR_SUCCESS, "could not open mmap");

		int i = 0;
		while(1){
			shmipc_error err = shmipc_send_message(sm, "test", "test", 6, 1);
			if(err == SHMIPC_ERR_SUCCESS){
				printf("wrote a message (%d)\n", i);
			}else if(err != SHMIPC_ERR_TIMEOUT){
				printf("error: %d\n", err);
				return 1;
			}
			i++;
		}

		shmipc_destroy(&sm);

	}else{
		shmipc* sm;

		printf("client\n");

		shmipc_error err = shmipc_create("test", 1024 * 1024 * 10, 32, SHMIPC_AM_READ, &sm);
		ASSERTMSG(err == SHMIPC_ERR_SUCCESS, "could not open mmap");

		while(1){
			Sleep(1000);
			size_t size;

			shmipc_error err = shmipc_recv_message(sm, type, buffer, &size, 1000);

			if(err == SHMIPC_ERR_SUCCESS){
				printf("got a message\n");
			}else if(err == SHMIPC_ERR_TIMEOUT){
				printf("no message\n");
			}else{
				printf("error: %d\n", err);
				return 1;
			}
		}

		shmipc_destroy(&sm);
	}


	return 0;
}
