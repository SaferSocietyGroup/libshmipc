#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "tests.h"
#include "platform.h"
#include "libshmipc.h"

struct ctx {
	shmstream* writer;
	shmstream* reader;

	int count;

	const char** msgs;
	int msg_count;
};

static unsigned int write_thread(void* vctx)
{
	struct ctx* ctx = (struct ctx*)vctx;

	for(int i = 0; i < ctx->count;){
		shmipc_error err = shmstream_write_pkt(ctx->writer, "test", ctx->msgs[i % ctx->msg_count], strlen(ctx->msgs[i % ctx->msg_count]));
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
	char* pkt;

	for(int i = 0; i < ctx->count;){
		size_t size = 0;
		shmipc_error err = shmstream_read_pkt(ctx->reader, type, &pkt, &size);

		if(err == SHMIPC_ERR_SUCCESS){
			size_t expects_size = strlen(ctx->msgs[i % ctx->msg_count]);

			ASSERT_RET_VAL(size == expects_size, 0, "incorrect message size (expected: %u, got %u)", expects_size, size);
			ASSERT_RET_VAL(strcmp(pkt, ctx->msgs[i % ctx->msg_count]) == 0, 0, "incorrect message, (expected: %s got %s)", ctx->msgs[i % ctx->msg_count], pkt);

			shmipc_free(pkt);

			i++;
		}

		else if(err != SHMIPC_ERR_TIMEOUT && err != SHMIPC_ERR_NO_DATA){
			printf("error: %d\n", err);
			return 0;
		}
	}

	return 1;
}

bool test_pkt_stream()
{
	shmipc_error e;
	struct ctx ctx;
	memset(&ctx, 0, sizeof(ctx));

	e = shmstream_create("test_pkt_stream", &ctx.writer);
	ASSERT_RET(e == SHMIPC_ERR_SUCCESS, "could not create packet stream");

	e = shmstream_open("test_pkt_stream", &ctx.reader);
	ASSERT_RET(e == SHMIPC_ERR_SUCCESS, "could not open packet stream");

	ctx.count = 10000;

	// Two big messages. The default message size in shmstream is 4096, so these two won't fit.
	char big_msg1[8192] = {0}, big_msg2[8193] = {0};

	memset(big_msg1, 'a', sizeof(big_msg1) - 1);
	memset(big_msg2, 'b', sizeof(big_msg2) - 1);

	const char* msgs[] = {"1", "2", "test", "12038012830180238091283019823", "jfjfjfjfjfas';la;sl", "asdq", big_msg1, big_msg2};
	ctx.msgs = msgs;
	ctx.msg_count = 8;

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
