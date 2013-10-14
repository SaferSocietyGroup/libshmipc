#ifndef LIBSHMIPC_H
#define LIBSHMIPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define SHMIPC_MESSAGE_TYPE_LENGTH 256
#define SHMIPC_INFINITE -1

typedef enum {
	SHMIPC_AM_READ  = 0,
	SHMIPC_AM_WRITE = 1
} shmipc_access_mode;

typedef enum {
	SHMIPC_ERR_SUCCESS,
	SHMIPC_ERR_TIMEOUT,
	SHMIPC_ERR_ALLOC,
	SHMIPC_ERR_OPEN_SHM,
	SHMIPC_ERR_CREATE_SEMAPHORE,
	SHMIPC_ERR_DOUBLE_ACQUIRE,
	SHMIPC_ERR_DOUBLE_RETURN,
	SHMIPC_ERR_WAIT_FAILED,
	SHMIPC_ERR_WRONG_MODE,
	SHMIPC_ERR_GET_SIZE,
	SHMIPC_ERR_UNKNOWN,
} shmipc_error;

typedef struct shmipc shmipc;
typedef struct shmhandle shmhandle;

shmipc_error shmipc_create(const char* name, size_t buffer_size, int buffer_count, shmipc_access_mode mode, shmipc** out_shmipc);
shmipc_error shmipc_open(const char* name, shmipc_access_mode mode, shmipc** out_shmipc);

shmipc_error shmipc_acquire_buffer_r(shmipc* me, char* out_type, const char** out_buffer, size_t* out_size, int timeout);
shmipc_error shmipc_return_buffer_r(shmipc* me, const char** buffer);

shmipc_error shmipc_acquire_buffer_w(shmipc* me, char** out_buffer, int timeout);
shmipc_error shmipc_return_buffer_w(shmipc* me, char** buffer, size_t size, const char* type);

shmipc_error shmipc_send_message(shmipc* me, const char* type, const char* message, size_t length, int timeout);
shmipc_error shmipc_recv_message(shmipc* me, char* out_type, char* out_message, size_t* out_size, int timeout);

void shmipc_destroy(shmipc** me);

shmipc_error shmipc_create_shm_rw(const char* name, size_t size, void** out_area, shmhandle** out_handle);
shmipc_error shmipc_open_shm_rw(const char* name, size_t* out_size, void** out_area, shmhandle** out_handle);

shmipc_error shmipc_create_shm_ro(const char* name, size_t size, const void** out_area, shmhandle** out_handle);
shmipc_error shmipc_open_shm_ro(const char* name, size_t* out_size, const void** out_area, shmhandle** out_handle);

void shmipc_destroy_shm(shmhandle** handle);

#ifdef __cplusplus
}
#endif

#endif
