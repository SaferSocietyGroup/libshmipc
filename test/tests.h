#ifndef TESTS_H
#define TESTS_H

#include <stdbool.h>

#define ASSERT_RET(_v, ...) if(!(_v)){ printf("error: " __VA_ARGS__); puts(""); return false; }
#define ASSERT_RET_VAL(_v, _val, ...) if(!(_v)){ printf("error: " __VA_ARGS__); puts(""); return _val; }

bool test_buffers(); 
bool test_pkt_stream();
bool test_shared_memory_area();

#endif
