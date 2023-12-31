#include <stdio.h>

#define CEVIL_IMPLEMENTATION
#include "cevil.h"

#define test(input, expected) test_pos(input, expected, __FILE__, __LINE__)

void test_pos(char* expr, double expected, const char* file, size_t line) {
	struct cevil_error err;
	double res;

	err = cevil_eval(expr, &res);

	if (err.type != CEVIL_ERROK) {
		printf("[FAILED]: ");
		fflush(stdout);
		cevil_print_error(&err);
		return;
	}

	if (res == expected) {
		printf("[PASSSED]: \"%s\" == %f\n", expr, res);
	}
	else {
		printf("[FAILED] %s:%zu: \"%s\" != %f (expected: %f)\n", file, line, expr, res, expected);
	}
}

int main(void) {
	test("1", 1);
	test("  1", 1);
	test("  1   ", 1);

	test("1 + 2", 1 + 2);
	test(" 1   +  2 ", 1 + 2);

	test("1 - 2", 1 - 2);

	test("69 * 2", 69 * 2);

	test("69 / 2", 69.0 / 2);

	test("1 * 2 + 2", 2 + 1 * 2);
	test("2 + 1 * 2", 2 + 1 * 2);

	test("2 / 1 * 2", 2 / 1 * 2);
	test("1 * 2 / 2", 1 * 2 / 2);

	test("19+21-2", 19+21-2);
}
