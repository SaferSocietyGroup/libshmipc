#include "libshmipc.h"
#include <string.h>

shmipc_error shmipc_send_message(shmipc* me, const char* type, const char* message, size_t length, int timeout)
{
	if(shmipc_get_buffer_size(me) < length + 1)
		return SHMIPC_ERR_MSG_TOO_LONG;

	char* buffer;
	shmipc_error err = shmipc_acquire_buffer_w(me, &buffer, timeout);

	if(err != SHMIPC_ERR_SUCCESS)
		return err;

	memcpy(buffer, message, length);
	buffer[length] = 0;

	printf("sending: %s\n", buffer);

	shmipc_return_buffer_w(me, &buffer, length + 1, type);

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

	return SHMIPC_ERR_SUCCESS;
}

