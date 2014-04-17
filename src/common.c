#include "libshmipc.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

shmipc_error shmipc_send_message(shmipc* me, const char* type, const char* message, size_t length, int timeout)
{
	if(shmipc_get_buffer_size(me) < length + 1)
		return SHMIPC_ERR_MSG_TOO_LONG;

	char* buffer;
	shmipc_error err = shmipc_acquire_buffer_w(me, &buffer, timeout);

	if(err != SHMIPC_ERR_SUCCESS)
		return err;

	memcpy(buffer, message, length);

	shmipc_return_buffer_w(me, &buffer, length, type);

	return SHMIPC_ERR_SUCCESS;
}

shmipc_error shmipc_recv_message(shmipc* me, char* out_type, char* out_message, size_t* out_size, int timeout)
{
	const char* buffer;
	size_t size;

	shmipc_error err = shmipc_acquire_buffer_r(me, out_type, &buffer, &size, timeout);

	if(err != SHMIPC_ERR_SUCCESS)
		return err;

	memcpy(out_message, buffer, size);
	shmipc_return_buffer_r(me, &buffer); 

	out_message[size] = 0;
	*out_size = size;

	return SHMIPC_ERR_SUCCESS;
}

struct shmstream
{
	shmipc* reader;
	shmipc* writer;
	bool rw_owned;
};

shmipc_error shmstream_create(const char* name, shmstream** out_stream)
{
	shmstream* me = calloc(1, sizeof(shmstream));
	if(!me)
		return SHMIPC_ERR_ALLOC;

	char* rname = calloc(1, strlen(name) + 16);
	if(!rname)
		return SHMIPC_ERR_ALLOC;

	char* wname = calloc(1, strlen(name) + 16);
	if(!wname)
		return SHMIPC_ERR_ALLOC;
	
	sprintf(rname, "%s_host_reader", name);
	sprintf(wname, "%s_host_writer", name);
	
	shmipc_error ret;
	ret = shmipc_create(wname, 4096, 4, SHMIPC_AM_WRITE, &me->writer);
	if(ret != SHMIPC_ERR_SUCCESS)
		return ret;

	ret = shmipc_create(rname, 4096, 4, SHMIPC_AM_READ, &me->reader);
	if(ret != SHMIPC_ERR_SUCCESS)
		return ret;

	me->rw_owned = true;

	*out_stream = me;

	return SHMIPC_ERR_SUCCESS;
}

shmipc_error shmstream_open(const char* name, shmstream** out_stream)
{
	shmstream* me = calloc(1, sizeof(shmstream));
	if(!me)
		return SHMIPC_ERR_ALLOC;

	char* rname = calloc(1, strlen(name) + 16);
	if(!rname)
		return SHMIPC_ERR_ALLOC;

	char* wname = calloc(1, strlen(name) + 16);
	if(!wname)
		return SHMIPC_ERR_ALLOC;
	
	// note, host writer is client reader and vice versa
	sprintf(rname, "%s_host_writer", name);
	sprintf(wname, "%s_host_reader", name);
	
	shmipc_error ret;
	ret = shmipc_open(wname, SHMIPC_AM_WRITE, &me->writer);
	if(ret != SHMIPC_ERR_SUCCESS)
		return ret;

	ret = shmipc_open(rname, SHMIPC_AM_READ, &me->reader);
	if(ret != SHMIPC_ERR_SUCCESS)
		return ret;

	me->rw_owned = true;

	*out_stream = me;

	return SHMIPC_ERR_SUCCESS;
}

shmipc_error shmstream_open_from(shmipc* reader, shmipc* writer, shmstream** out_stream)
{
	shmstream* me = calloc(1, sizeof(shmstream));
	if(!me)
		return SHMIPC_ERR_ALLOC;
	
	me->reader = reader;
	me->writer = writer;

	*out_stream = me;

	return SHMIPC_ERR_SUCCESS;
}

void shmstream_destroy(shmstream** me)
{
	assert(me);
	if((*me)->rw_owned){
		shmipc_destroy(&(*me)->reader);
		shmipc_destroy(&(*me)->writer);
	}

	free(*me);
	*me = NULL;
}

shmipc_error shmstream_read_pkt(shmstream* me, char* out_type, char** out_pkt, size_t* out_size)
{
	assert(me);

	char tmp[shmipc_get_message_max_length(me->reader)];
	size_t read_size;

	// get the size of the packet (or return NO_DATA if none is available)
	shmipc_error e = shmipc_recv_message(me->reader, out_type, tmp, &read_size, 0);
	if(e != SHMIPC_ERR_SUCCESS)
		return e == SHMIPC_ERR_TIMEOUT ? SHMIPC_ERR_NO_DATA : e;
	
	assert(read_size == sizeof(uint64_t));
	
	size_t size = (size_t)*((uint64_t*)tmp);

	char* buffer = NULL;
	
	// allocate buffer (if packet size is larger than 0)
	if(size > 0){
		buffer = malloc(size);
		if(!buffer)
			return SHMIPC_ERR_ALLOC;
	}

	char* rb = buffer;
	
	// set the outputs
	*out_size = (size_t)size;
	*out_pkt = buffer;

	// read all buffers until the whole packet has been read
	while(size > 0){
		e = shmipc_recv_message(me->reader, out_type, rb, &read_size, SHMIPC_INFINITE);

		if(e != SHMIPC_ERR_SUCCESS){
			free(buffer);
			return e;
		}
		
		size -= read_size;
		rb += read_size;
	}

	return SHMIPC_ERR_SUCCESS;
}

shmipc_error shmstream_write_pkt(shmstream* me, const char* type, const char* buffer, size_t size)
{
	size_t buffer_size = shmipc_get_buffer_size(me->writer);
	uint64_t u64size = size;
	
	shmipc_error e = shmipc_send_message(me->writer, type, (char*)&u64size, sizeof(uint64_t), SHMIPC_INFINITE);
	if(e != SHMIPC_ERR_SUCCESS)
		return e;

	while(size > 0){
		size_t write_size = size < buffer_size ? size : buffer_size;

		char* wbuffer = NULL;

		e = shmipc_acquire_buffer_w(me->writer, &wbuffer, SHMIPC_INFINITE);
		if(e != SHMIPC_ERR_SUCCESS)
			return e;

		memcpy(wbuffer, buffer, write_size);
		
		e = shmipc_return_buffer_w(me->writer, &wbuffer, write_size, type);
		if(e != SHMIPC_ERR_SUCCESS)
			return e;

		size -= write_size;
		buffer += write_size;
	}

	return SHMIPC_ERR_SUCCESS;
}
