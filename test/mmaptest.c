#include <stdbool.h>
#include <stdio.h>

#include "tests.h"

struct test {
	const char* name;
	bool (*fn)();
};

#define DEF_TEST(_t) { #_t, _t }

int main(int argc, char** argv)
{
	struct test tests[] = {
		DEF_TEST(test_buffers),
		DEF_TEST(test_pkt_stream),
		DEF_TEST(test_shared_memory_area)
	};
	
	int ok = 0;
	int num_tests = sizeof(tests) / sizeof(struct test);

	printf("Running %d test(s).\n", num_tests);

	for(int i = 0; i < num_tests; i++){
		printf("  [%2d/%2d] %-25s", i + 1, num_tests, tests[i].name);
		bool result = tests[i].fn();
		printf("%s\n", result ? "ok" : "failed");
		ok += result ? 1 : 0;
	}

	printf("\n%d/%d tests succeeded\n", ok, num_tests);

	return ok == num_tests ? 0 : 1;
}
