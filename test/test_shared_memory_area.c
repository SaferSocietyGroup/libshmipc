#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "tests.h"
#include "platform.h"
#include "libshmipc.h"

struct ctx {
	shmhandle* reader_handle;
	shmhandle* writer_handle;

	const char* reader;
	char* writer;

	bool done;
	mr_mutex* mutex;

	char* data;
	size_t data_size;
};

static unsigned int write_thread(void* vctx)
{
	struct ctx* ctx = (struct ctx*)vctx;
	mr_lock_mutex(ctx->mutex);
	memcpy(ctx->writer, ctx->data, ctx->data_size);
	mr_unlock_mutex(ctx->mutex);
	//printf("unlocked\n");
	return 1;
}

static unsigned int read_thread(void* vctx)
{
	struct ctx* ctx = (struct ctx*)vctx;
	mr_sleep(100);

	mr_lock_mutex(ctx->mutex);

	int ret = memcmp(ctx->reader, ctx->data, ctx->data_size);
	if(ret != 0){
		printf("data mismatch!\n");
	}

	mr_unlock_mutex(ctx->mutex);

	return ret == 0;
}

bool test_shared_memory_area()
{
	shmipc_error e;
	struct ctx ctx;

	ctx.data_size = 1024 * 1024 * 10;
	ctx.data = calloc(1, ctx.data_size);

	for(size_t i = 0; i < ctx.data_size; i++)
		ctx.data[i] = i % 256;

	ctx.mutex = mr_create_mutex();
	//mr_lock_mutex(ctx.mutex);
	
	e = shmipc_create_shm_rw("test_area", ctx.data_size, (void**)&ctx.writer, &ctx.writer_handle);
	ASSERT_RET(e == SHMIPC_ERR_SUCCESS, "could not create shm area for writing");
	
	size_t size;
	e = shmipc_open_shm_ro("test_area", &size, (const void**)&ctx.reader, &ctx.reader_handle);
	ASSERT_RET(e == SHMIPC_ERR_SUCCESS, "could not create shm area for reading");
	ASSERT_RET(size == ctx.data_size, "expected size %u, but got %u", size, ctx.data_size);
	
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
