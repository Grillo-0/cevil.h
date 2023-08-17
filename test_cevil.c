#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#define CEVIL_IMPLEMENTATION
#include "cevil.h"

#define test(input, expected) test_pos(input, expected, __FILE__, __LINE__)

void test_pos(char* expr, double expected, const char* file, size_t line) {
	double res = cevil_eval(expr);

	if (res == expected) {
		printf("[PASSSED]: \"%s\" == %f\n", expr, res);
	}
	else {
		printf("[FAILED] %s:%zu: \"%s\" != %f\n", file, line, expr, res);
		exit(-1);
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
}
