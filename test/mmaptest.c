#include <libshmipc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define ASSERTMSG(_v, ...) if(!(_v)){ printf(__VA_ARGS__); puts(""); exit(1); }

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN

#include <windows.h>

#define TESTSTRING "test"

void queue_open()
{
	shmipc* sm;

	printf("queue open\n");

	shmipc_error err = shmipc_open("test", SHMIPC_AM_WRITE, &sm);
	ASSERTMSG(err == SHMIPC_ERR_SUCCESS, "could not open mmap");

	int i = 0;
	while(1){
		shmipc_error err = shmipc_send_message(sm, TESTSTRING, TESTSTRING, strlen(TESTSTRING), 1);
		if(err == SHMIPC_ERR_SUCCESS){
			printf("wrote a message (%d)\n", i);
		}else if(err != SHMIPC_ERR_TIMEOUT){
			printf("error: %d\n", err);
			exit(1);
		}
		i++;
	}

	shmipc_destroy(&sm);
}

void queue_create()
{
	char type[256];
	char* buffer = malloc(1024 * 1024 * 10);
	shmipc* sm;

	printf("queue create\n");

	shmipc_error err = shmipc_create("test", 1024 * 1024 * 10, 32, SHMIPC_AM_READ, &sm);
	ASSERTMSG(err == SHMIPC_ERR_SUCCESS, "could not open mmap");

	while(1){
		Sleep(1000);
		size_t size;

		shmipc_error err = shmipc_recv_message(sm, type, buffer, &size, 1000);

		if(err == SHMIPC_ERR_SUCCESS){
			printf("got a message: '%s' (%d)\n", buffer, size);
			ASSERTMSG(size == strlen(TESTSTRING), "incorrect message size");
			ASSERTMSG(strcmp(buffer, TESTSTRING) == 0, "incorrect message");

		}else if(err == SHMIPC_ERR_TIMEOUT){
			printf("no message\n");
		}else{
			printf("error: %d\n", err);
			exit(1);
		}
	}

	shmipc_destroy(&sm);
}

void shm_open()
{
	const char* area;
	shmhandle* handle;
	size_t size;

	printf("shm open\n");

	shmipc_error err = shmipc_open_shm_ro("test", &size, (const void**)&area, &handle);
	ASSERTMSG(err == SHMIPC_ERR_SUCCESS, "could not open shm");

	// will return the size of the area actually mapped, so it probably differs from the size specified
	ASSERTMSG(size >= 256, "unexpected view size: %lu", (long)size);
	ASSERTMSG(!strcmp(area, "hello"), "unexpected contents of area");

	shmipc_destroy_shm(&handle);
}

void shm_create()
{
	char* area;
	shmhandle* handle;

	printf("shm create\n");

	shmipc_error err = shmipc_create_shm_rw("test", 256, (void**)&area, &handle);
	ASSERTMSG(err == SHMIPC_ERR_SUCCESS, "could not create shm");

	strcpy(area, "hello");

	printf("press enter to quit\n");
	while(fgetc(stdin) != '\n'){}

	shmipc_destroy_shm(&handle);
}

void tshmstream_create()
{
	shmstream* s;
	shmipc_error e = shmstream_create("test", &s);
	ASSERTMSG(e == SHMIPC_ERR_SUCCESS, "could not create stream");

	for(int i = 0; i < 10; i++){
		char buffer[12345] = {0};
		for(int j = 0; j < sizeof(buffer); j++)
			buffer[j] = (char)(i + j);

		e = shmstream_write_pkt(s, "test", buffer, sizeof(buffer));
		ASSERTMSG(e == SHMIPC_ERR_SUCCESS, "could not write packet");
	}
}

void tshmstream_open()
{
	char type[SHMIPC_MESSAGE_TYPE_LENGTH];
	shmstream* s;
	shmipc_error e = shmstream_open("test", &s);
	ASSERTMSG(e == SHMIPC_ERR_SUCCESS, "could not create stream");

	for(int i = 0; i < 10; i++){
		char* buffer;
		size_t size;

		e = shmstream_read_pkt(s, type, &buffer, &size);
		ASSERTMSG(e == SHMIPC_ERR_SUCCESS, "could not read packet (%d)", e);

		ASSERTMSG(size == 12345, "unexpected packet size");

		for(int j = 0; j < size; j++)
			ASSERTMSG(buffer[j] == (char)(i + j), "unexpected packet contents");
	}
}

int main(int argc, char** argv)
{
	ASSERTMSG(argc == 3, "usage: %s [queue/shm] [open/create]", argv[0]);
	
	bool open = !strcmp(argv[2], "open");

	if(!strcmp(argv[1], "queue")){
		if(open)
			queue_open();
		else
			queue_create();
	}

	else if(!strcmp(argv[1], "shm")){
		if(open)
			shm_open();
		else
			shm_create();
	}
	
	else if(!strcmp(argv[1], "stream")){
		if(open)
			tshmstream_open();
		else
			tshmstream_create();
	}

	else{
		ASSERTMSG(0, "huh?");
	}

	return 0;
}
