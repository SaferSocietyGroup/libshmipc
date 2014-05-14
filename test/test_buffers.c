#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "tests.h"
#include "platform.h"
#include "libshmipc.h"

struct ctx {
	shmipc* writer;
	shmipc* reader;

	int count;

	const char** msgs;
	int msg_count;
};

static unsigned int write_thread(void* vctx)
{
	struct ctx* ctx = (struct ctx*)vctx;

	for(int i = 0; i < ctx->count;){
		shmipc_error err = shmipc_send_message(ctx->writer, "test", ctx->msgs[i % ctx->msg_count], strlen(ctx->msgs[i % ctx->msg_count]), 1);
		ASSERT_RET_VAL(err == SHMIPC_ERR_SUCCESS || err == SHMIPC_ERR_TIMEOUT, 0, "could not write to message queue");

		if(err != SHMIPC_ERR_TIMEOUT)
			i++;
	}

	return 1;
}

static unsigned int read_thread(void* vctx)
{
	struct ctx* ctx = (struct ctx*)vctx;

	char type[SHMIPC_MESSAGE_TYPE_LENGTH];
	char buffer[512];

	for(int i = 0; i < ctx->count;){
		size_t size = 0;
		shmipc_error err = shmipc_recv_message(ctx->reader, type, buffer, &size, 512);

		if(err == SHMIPC_ERR_SUCCESS){
			//printf("got a message: '%s' (%d)\n", buffer, size);

			size_t expects_size = strlen(ctx->msgs[i % ctx->msg_count]);

			ASSERT_RET_VAL(size == expects_size, 0, "incorrect message size (expected: %u, got %u)", expects_size, size);
			ASSERT_RET_VAL(strcmp(buffer, ctx->msgs[i % ctx->msg_count]) == 0, 0, "incorrect message, (expected: %s got %s)", ctx->msgs[i % ctx->msg_count], buffer);

			i++;
		}else if(err == SHMIPC_ERR_TIMEOUT){
			printf("no message\n");
		}else{
			printf("error: %d\n", err);
			return 0;
		}
	}

	return 1;
}

bool test_buffers()
{
	shmipc_error err;
	struct ctx ctx;
	memset(&ctx, 0, sizeof(ctx));

	ctx.count = 10000;
	const char* msgs[] = {"1", "2", "test", "12038012830180238091283019823", "jfjfjfjfjfas';la;sl", "asdq"};
	ctx.msgs = msgs;
	ctx.msg_count = 6;

	// create a reader and a writer
	err = shmipc_create("test_buffers", 1024 * 1024 * 10, 32, SHMIPC_AM_WRITE, &ctx.writer);
	ASSERT_RET(err == SHMIPC_ERR_SUCCESS, "could not create mmap (%d)", err);
	
	err = shmipc_open("test_buffers", SHMIPC_AM_READ, &ctx.reader);
	ASSERT_RET(err == SHMIPC_ERR_SUCCESS, "could not open mmap (%d)", err);

	// start reader/writer threads
	mr_thread* wt = mr_create_thread(write_thread, &ctx);
	mr_thread* rt = mr_create_thread(read_thread, &ctx);

	// wait for threads to finish
	unsigned int wtr = mr_wait_thread(wt);
	unsigned int rtr = mr_wait_thread(rt);

	// check their return values
	ASSERT_RET(wtr, "write thread failed");
	ASSERT_RET(rtr, "read thread failed");

	return true;
}
