#ifdef _WIN32

#include "libshmipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdbool.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

typedef struct __attribute__ ((packed)) {
	uint32_t size;
	uint32_t count;
} s_header;

typedef struct __attribute__ ((packed)) {
	char type[SHMIPC_MESSAGE_TYPE_LENGTH];
	uint32_t length;
} s_message_header;

struct shmipc
{
	s_header header;

	wchar_t* name;
	int buffer_pos;
	char** buffers;
	bool buffer_in_use;
	shmipc_access_mode mode;
	HANDLE read_sem, write_sem;
	HANDLE file;
};

struct shmhandle
{
	HANDLE file;
	void* view;
};

// asserts that a evaluates as true, otherwise exits the application with the error message msg
//#define ASSERTMSG(a, ...) if(!(a)){ printf(__VA_ARGS__); puts(""); exit(1); }

// Implementation inspired by http://comsci.liu.edu/~murali/win32/SharedMemory.htm

#define ACTUAL_SIZE(_s) (sizeof(s_message_header) + (_s))

static wchar_t* str_to_wstr(const char* str)
{
	int len = strlen(str);

	wchar_t* ret = (wchar_t*)calloc(1, (len + 1) * sizeof(wchar_t));

	if(ret){
		for(int i = 0; i < len; i++)
			ret[i] = str[i];
	}

	return ret;
}

static shmipc_error create_or_open_shm(const char* name, size_t* in_out_size, void** out_area, shmhandle** out_handle, bool open, bool rw)
{
	shmipc_error ret = SHMIPC_ERR_UNKNOWN;

	shmhandle* handle = calloc(1, sizeof(shmhandle));
	if(!handle)
		return SHMIPC_ERR_ALLOC;
		 
	wchar_t* wname = str_to_wstr(name);
	if(!wname)
		return SHMIPC_ERR_ALLOC;

	if(!open){
		// TODO very large areas, above 4GB  
		handle->file = CreateFileMappingW(
				INVALID_HANDLE_VALUE,                   // use paging file
				NULL,                                   // default security
				PAGE_READWRITE,                         // read/write access
				0,                                      // size of memory area (high-order DWORD)
				*in_out_size,                           // size of memory area (low-order DWORD)
				wname);                                 // name of mapping object
	}else{
		handle->file = OpenFileMappingW(
				rw ? FILE_MAP_WRITE : FILE_MAP_READ,   // read/write access
				FALSE,            // do not inherit the name
				wname);           // name of mapping object
	}

	free(wname);
	
	if(!handle->file){
		printf("1\n");
		ret = SHMIPC_ERR_OPEN_SHM;
		goto cleanup;
	}
	
	// map the view
	handle->view = MapViewOfFile(handle->file, rw ? FILE_MAP_WRITE : FILE_MAP_READ, 0, 0, 0);

	if(!handle->view){
		printf("3\n");
		ret = SHMIPC_ERR_OPEN_SHM;
		goto cleanup;
	}

	*out_handle = handle;
	*out_area = handle->view;

	if(open){
		MEMORY_BASIC_INFORMATION info;
		if(VirtualQuery(handle->view, &info, sizeof(MEMORY_BASIC_INFORMATION)) == 0){
			printf("4\n");
			ret = SHMIPC_ERR_GET_SIZE;
			goto cleanup;
		}
		*in_out_size = info.RegionSize;
	}

	return SHMIPC_ERR_SUCCESS;

cleanup:
	// TODO close mapping on error
	free(handle);
	return ret;
}

shmipc_error shmipc_create_shm_rw(const char* name, size_t size, void** out_area, shmhandle** out_handle)
{
	return create_or_open_shm(name, &size, out_area, out_handle, false, true);
}

shmipc_error shmipc_open_shm_rw(const char* name, size_t* out_size, void** out_area, shmhandle** out_handle)
{
	return create_or_open_shm(name, out_size, out_area, out_handle, true, true);
}

shmipc_error shmipc_create_shm_ro(const char* name, size_t size, const void** out_area, shmhandle** out_handle)
{
	return create_or_open_shm(name, &size, (void**)out_area, out_handle, true, false);
}

shmipc_error shmipc_open_shm_ro(const char* name, size_t* out_size, const void** out_area, shmhandle** out_handle)
{
	return create_or_open_shm(name, out_size, (void**)out_area, out_handle, true, false);
}

void shmipc_destroy_shm(shmhandle** handle)
{
	UnmapViewOfFile((*handle)->view);

	CloseHandle((*handle)->file);
	free(*handle);
	*handle = NULL;
}

int shmipc_get_message_max_length(shmipc* me)
{
	return me->header.size;
}

static shmipc_error create_or_open(const char* name, uint32_t buffer_size, uint32_t buffer_count, shmipc_access_mode mode, bool open, shmipc** out_me)
{
	shmipc_error err = SHMIPC_ERR_UNKNOWN;

	shmipc* me = (shmipc*)calloc(1, sizeof(shmipc));

	if(!me)
		return SHMIPC_ERR_ALLOC;

	me->header.count = buffer_count;
	me->header.size = buffer_size;
	me->mode = mode;
	me->name = str_to_wstr(name);
	
	if(!me->name){
		err = SHMIPC_ERR_ALLOC;
		goto cleanup;
	}

	if(open){
		// Open the existing "file"
		printf("opening existing shared memory file: '%s'\n", name);
		me->file = OpenFileMappingW(
				FILE_MAP_WRITE, // read/write access
				FALSE,          // do not inherit the name
				me->name);          // name of mapping object
		
		if(!me->file){
			err = SHMIPC_ERR_OPEN_SHM;
			goto cleanup;
		}

		// Temporarily map a view of the file to get buffer sizes etc.	
		char* buffer = (char*)MapViewOfFile(me->file, FILE_MAP_WRITE, 0, 0, sizeof(s_header));

		if(!buffer){
			err = SHMIPC_ERR_OPEN_SHM;
			goto cleanup;
		}

		//ASSERTMSG(buffer, "could not map view of file: (%ld)", (long int)GetLastError());

		memcpy(&me->header, buffer, sizeof(s_header));
		UnmapViewOfFile(buffer);

		printf("Opened file with %u buffers of size %u\n", me->header.count, me->header.size);
	}else{
		// Create a new "file"
		printf("creating new shared memory file: '%s'\n", name);
		me->file = CreateFileMappingW(
				INVALID_HANDLE_VALUE,     // use paging file
				NULL,                     // default security
				PAGE_READWRITE,           // read/write access
				0,                        // size of memory area (high-order DWORD)
				sizeof(s_header) + ACTUAL_SIZE(buffer_size) * buffer_count, // size of memory area (low-order DWORD)
				me->name);                    // name of mapping object
	}

	//ASSERTMSG(me->file, "failed to create/open file mapping (%lu)", (unsigned long)GetLastError());
	if(!me->file){
		err = SHMIPC_ERR_OPEN_SHM;
		goto cleanup;
	}

	// Map the views of the file
	me->buffers = (char**)calloc(sizeof(char*), me->header.count);

	char* mem = (char*)MapViewOfFile(me->file, FILE_MAP_WRITE, 0, 0, ACTUAL_SIZE(me->header.size) * me->header.count + sizeof(s_header));
	//FlogExpD(ACTUAL_SIZE(me->header.size) * me->header.count + sizeof(header));

	//ASSERTMSG(mem, "could not map view of file (%lu)", (unsigned long)GetLastError());
	if(!mem){
		err = SHMIPC_ERR_OPEN_SHM;
		goto cleanup;
	}

	if(!open)
		memcpy(mem, me, sizeof(s_header));

	for(unsigned int i = 0; i < me->header.count; i++){
		me->buffers[i] = mem + sizeof(s_header) + i * ACTUAL_SIZE(me->header.size);
	}

	char tmp_name[512];

	// Create the read semaphore
	sprintf(tmp_name, "%s_read", name);
	wchar_t* lname = str_to_wstr(tmp_name);
	me->read_sem = CreateSemaphoreW(NULL, 0, me->header.count, lname);
	//	"could not create semaphore (%lu)", (unsigned long)GetLastError());
	free(lname);

	if(!me->read_sem){
		err = SHMIPC_ERR_CREATE_SEMAPHORE;
		goto cleanup;
	}
	
	// Create the write semaphore
	sprintf(tmp_name, "%s_write", name);
	lname = str_to_wstr(tmp_name);
	me->write_sem = CreateSemaphoreW(NULL, me->header.count, me->header.count, lname);
	//	"could not create semaphore (%lu)", (unsigned long)GetLastError());
	free(lname);
	
	if(!me->write_sem){
		err = SHMIPC_ERR_CREATE_SEMAPHORE;
		goto cleanup;
	}

	*out_me = me;
	return SHMIPC_ERR_SUCCESS;

cleanup:
	if(me)
		free(me);

	return err;
}

static shmipc_error acquire_buffer(shmipc* me, int timeout, char** buffer)
{
	if(me->buffer_in_use)
		return SHMIPC_ERR_DOUBLE_ACQUIRE;

	DWORD ret = WaitForSingleObject(me->mode == SHMIPC_AM_WRITE ? me->write_sem : me->read_sem, timeout == SHMIPC_INFINITE ? INFINITE : timeout);

	if(ret == 0){
		me->buffer_in_use = true;

		*buffer = me->buffers[me->buffer_pos++ % me->header.count];
		return SHMIPC_ERR_SUCCESS;
	}

	return ret == WAIT_TIMEOUT ? SHMIPC_ERR_TIMEOUT : SHMIPC_ERR_WAIT_FAILED;
}

shmipc_error shmipc_acquire_buffer_r(shmipc* me, char* out_type, const char** out_buffer, size_t* out_size, int timeout)
{
	if(me->mode != SHMIPC_AM_READ)
		return SHMIPC_ERR_WRONG_MODE;

	//ASSERTMSG(me->mode == SHMIPC_AM_READ, "trying to acquire a read buffer from a write mmap");

	char* buffer;
	shmipc_error err = acquire_buffer(me, timeout, &buffer);
	if(err != SHMIPC_ERR_SUCCESS)
		return err;

	s_message_header* header = (s_message_header*)buffer;

	memcpy(out_type, header->type, SHMIPC_MESSAGE_TYPE_LENGTH);
	*out_size  = header->length;
	*out_buffer = buffer + sizeof(s_message_header);

	return SHMIPC_ERR_SUCCESS;
}

shmipc_error shmipc_acquire_buffer_w(shmipc* me, char** out_buffer, int timeout)
{
	if(me->mode != SHMIPC_AM_WRITE)
		return SHMIPC_ERR_WRONG_MODE;

	//ASSERTMSG(me->mode == SHMIPC_AM_WRITE, "trying to acquire a write buffer from a read mmap");

	
	char* buffer;
	shmipc_error err = acquire_buffer(me, timeout, &buffer);
	if(err != SHMIPC_ERR_SUCCESS)
		return err;

	*out_buffer = buffer + sizeof(s_message_header);
	return SHMIPC_ERR_SUCCESS;
}

shmipc_error shmipc_return_buffer(shmipc* me)
{
	//ASSERTMSG(me->buffer_in_use, "trying to return buffer twice");
	if(!me->buffer_in_use)
		return SHMIPC_ERR_DOUBLE_RETURN;

	me->buffer_in_use = false;
	ReleaseSemaphore(me->mode == SHMIPC_AM_WRITE ? me->read_sem : me->write_sem, 1, NULL);

	return SHMIPC_ERR_SUCCESS;
}


shmipc_error shmipc_return_buffer_w(shmipc* me, char** buffer, uint32_t length, const char* type)
{
	char* real_buf = (*buffer) -= sizeof(s_message_header);
	s_message_header* header = (s_message_header*)real_buf;

	header->length = length;
	strncpy(header->type, type, SHMIPC_MESSAGE_TYPE_LENGTH);

	shmipc_error err = shmipc_return_buffer(me);
	if(err != SHMIPC_ERR_SUCCESS)
		return err;

	*buffer = NULL;
	return SHMIPC_ERR_SUCCESS;
}

shmipc_error shmipc_return_buffer_r(shmipc* me, const char** buffer)
{
	shmipc_error err = shmipc_return_buffer(me);
	if(err != SHMIPC_ERR_SUCCESS)
		return err;

	*buffer = NULL;
	return SHMIPC_ERR_SUCCESS;
}

shmipc_error shmipc_open(const char* name, shmipc_access_mode mode, shmipc** shmipc)
{
	return create_or_open(name, 0, 0, mode, true, shmipc);
}

shmipc_error shmipc_create(const char* name, size_t buffer_size, int buffer_count, shmipc_access_mode mode, shmipc** shmipc)
{
	return create_or_open(name, buffer_size, buffer_count, mode, false, shmipc);
}

void shmipc_destroy(shmipc** me)
{
	for(unsigned int i = 0; i < (*me)->header.count; i++){
		UnmapViewOfFile((*me)->buffers[i]);
	}

	CloseHandle((*me)->file);
	free((*me)->name);
	free(*me);
	*me = NULL;
}

size_t shmipc_get_buffer_size(shmipc* me)
{
	return me->header.size;
}

int shmipc_get_buffer_count(shmipc* me)
{
	return me->header.count;
}

#endif
