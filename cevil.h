#ifndef CEVIL_H_
#define CEVIL_H_

#define CEVIL_ERROR_BUF_SIZE 256

struct cevil_error {
	enum {
		CEVIL_ERROK,
		CEVIL_ERRPAR,
		CEVIL_ERREVAL,
		CEVIL_ERRGEN,
	} type;
	char msg[CEVIL_ERROR_BUF_SIZE];
};

struct cevil_error cevil_eval(const char *expr, double *result);
void cevil_print_error(struct cevil_error *err);

#endif // CEVIL_H_

#ifdef CEVIL_IMPLEMENTATION

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define cevil_ok (struct cevil_error){.type = CEVIL_ERROK}

typedef size_t tkid_t;

enum cevil_tk_type {
	CEVIL_NUM_TK,
	CEVIL_PLUS_TK,
	CEVIL_MINUS_TK,
	CEVIL_MULT_TK,
	CEVIL_DIV_TK,
	CEVIL_TK_SIZE,
};

struct cevil_tk;

typedef union {
	double number;
	struct {
		tkid_t rhs;
		tkid_t lhs;
	};
} cevil_as;

struct cevil_tk {
	enum cevil_tk_type type;
	cevil_as as;
	bool evaluated;
	double value;
};

struct tk_storage {
	struct cevil_tk *arr;
	size_t len;
	size_t capacity;
};

struct cevil_expr {
	const char *base;
	const char *parser_cursor;
	tkid_t root;
	struct tk_storage stg;
};

void cevil_print_error(struct cevil_error *err) {
	switch (err->type) {
	case CEVIL_ERROK:
		fprintf(stderr, "No error\n");
		break;
	case CEVIL_ERRPAR:
		fprintf(stderr, "Parser error:");
		break;
	case CEVIL_ERREVAL:
		fprintf(stderr, "Eval error:");
		break;
	case CEVIL_ERRGEN:
		fprintf(stderr, "Generic error:");
		break;
	}

	fprintf(stderr, "%*.s\n", CEVIL_ERROR_BUF_SIZE - 1, err->msg);
}

static tkid_t tk_storage_alloc(struct tk_storage *stg) {
	stg->len++;

	if (stg->len > stg->capacity) {
		if (stg->capacity == 0)
			stg->capacity = 128;

		stg->capacity *= 2;
		stg->arr = realloc(stg->arr, sizeof(*stg) * stg->capacity);
		assert(stg->arr != NULL);
	}

	return stg->len - 1;
}

static struct cevil_tk* tk_storage_get(struct tk_storage stg, tkid_t index) {
	assert(index < stg.len);

	return &stg.arr[index];
}

static void tk_storage_free(struct tk_storage *stg) {
	free(stg->arr);
	stg->len = 0;
	stg->capacity = 0;
}

static void cevil_expr_init(struct cevil_expr *expr, const char *str) {
	memset(expr, 0, sizeof(*expr));


	char *ptr = calloc(sizeof(*str), strlen(str) + 1);
	assert(ptr != NULL && "Buy more RAM LOL");
	memcpy(ptr, str, strlen(str) * sizeof(*str));

	expr->base = ptr;
	expr->parser_cursor = expr->base;
}

static void cevil_expr_free(struct cevil_expr *expr) {
	free((char*)expr->base);
	tk_storage_free(&expr->stg);
}

void chop(const char **str) {
	while(**str != '\0' && isspace(**str))
		(*str)++;
}

static bool is_binary_op(enum cevil_tk_type tk) {
	static bool is_binary_op_table[CEVIL_TK_SIZE] = {
		[CEVIL_PLUS_TK] = true,
		[CEVIL_MINUS_TK] = true,
		[CEVIL_MULT_TK] = true,
		[CEVIL_DIV_TK] = true,
	};

	return is_binary_op_table[tk];
}

/**
 * Get the precedence value of an operation
 * @tk: token to the precedence value
 *
 * Returns:
 *
 * Returns the precedence value or 0 if the token is not a operation.
 * The bigger the number, the early the operation needs to be
 * evaluated.
 */
static size_t precedence(enum cevil_tk_type tk) {
	static size_t precedence_table[CEVIL_TK_SIZE] = {
		[CEVIL_PLUS_TK] = 1,
		[CEVIL_MINUS_TK] = 1,
		[CEVIL_MULT_TK] = 2,
		[CEVIL_DIV_TK] = 2,
	};

	return precedence_table[tk];
}

struct cevil_error next_tk(struct cevil_expr *expr, tkid_t *tk_id) {
	*tk_id = tk_storage_alloc(&expr->stg);
	struct cevil_tk *tk = tk_storage_get(expr->stg, *tk_id);
	tk->evaluated = false;

	char* end;

	double value = strtod(expr->parser_cursor, &end);

	if (end != expr->parser_cursor) {
		tk->type = CEVIL_NUM_TK;
		tk->as.number = value;
		expr->parser_cursor = end;
	} else if (*expr->parser_cursor == '+') {
		tk->type = CEVIL_PLUS_TK;
		expr->parser_cursor++;
	} else if (*expr->parser_cursor == '-') {
		tk->type = CEVIL_MINUS_TK;
		expr->parser_cursor++;
	} else if (*expr->parser_cursor == '*') {
		tk->type = CEVIL_MULT_TK;
		expr->parser_cursor++;
	} else if (*expr->parser_cursor == '/') {
		tk->type = CEVIL_DIV_TK;
		expr->parser_cursor++;
	} else {
		struct cevil_error err = {.type = CEVIL_ERRPAR};
		snprintf(err.msg, CEVIL_ERROR_BUF_SIZE,
			 "error: Unexpected charcter '%c'\n", *expr->base);
		return err;
	}

	if (is_binary_op(tk->type)) {
		struct cevil_error err;

		err = next_tk(expr, &tk->as.rhs);

		struct cevil_tk *root = tk_storage_get(expr->stg, expr->root);

		if (is_binary_op(root->type) &&
		    precedence(tk->type) > precedence(root->type)) {
			tk->as.lhs = root->as.rhs;
			root->as.rhs = *tk_id;
			*tk_id = expr->root;
		} else {
			tk->as.lhs = expr->root;
		}

		if (err.type != CEVIL_ERROK)
			return err;
	};

	return cevil_ok;
}

void eval(tkid_t root_id, struct tk_storage stg) {
	struct cevil_tk *root = tk_storage_get(stg, root_id);

	struct cevil_tk *rhs = NULL;
	struct cevil_tk *lhs = NULL;

	switch (root->type) {
	case CEVIL_NUM_TK:
		root->evaluated = true;
		root->value = root->as.number;
		break;
	case CEVIL_PLUS_TK:
		rhs = tk_storage_get(stg, root->as.rhs);
		lhs = tk_storage_get(stg, root->as.lhs);

		eval(root->as.rhs, stg);
		eval(root->as.lhs, stg);

		root->evaluated = lhs->evaluated && rhs->evaluated;

		if (!root->evaluated)
			return;

		root->value = lhs->value + rhs->value;
		break;
	case CEVIL_MINUS_TK:
		rhs = tk_storage_get(stg, root->as.rhs);
		lhs = tk_storage_get(stg, root->as.lhs);

		eval(root->as.rhs, stg);
		eval(root->as.lhs, stg);

		root->evaluated = lhs->evaluated && rhs->evaluated;

		if (!root->evaluated)
			return;

		root->value = lhs->value - rhs->value;
		break;
	case CEVIL_MULT_TK:
		rhs = tk_storage_get(stg, root->as.rhs);
		lhs = tk_storage_get(stg, root->as.lhs);

		eval(root->as.rhs, stg);
		eval(root->as.lhs, stg);

		root->evaluated = lhs->evaluated && rhs->evaluated;

		if (!root->evaluated)
			return;

		root->value = lhs->value * rhs->value;
		break;
	case CEVIL_DIV_TK:
		rhs = tk_storage_get(stg, root->as.rhs);
		lhs = tk_storage_get(stg, root->as.lhs);

		eval(root->as.rhs, stg);
		eval(root->as.lhs, stg);

		root->evaluated = lhs->evaluated && rhs->evaluated;

		if (!root->evaluated)
			return;

		root->value = lhs->value / rhs->value;
		break;
	case CEVIL_TK_SIZE:
	default:
		assert(false && "Unreachable");
		break;
	}
}

struct cevil_error cevil_eval(const char *input, double *result) {
	struct cevil_expr expr;
	cevil_expr_init(&expr, input);

	chop(&expr.parser_cursor);
	while (*expr.parser_cursor != '\0') {
		struct cevil_error err;
		tkid_t result;
		err = next_tk(&expr, &result);
		expr.root = result;
		if (err.type != CEVIL_ERROK)
			return err;
		chop(&expr.parser_cursor);
	}

	eval(expr.root, expr.stg);

	struct cevil_tk *root = tk_storage_get(expr.stg, expr.root);

	if (root->evaluated == false) {
		struct cevil_error err = {.type = CEVIL_ERREVAL};
		snprintf(err.msg, CEVIL_ERROR_BUF_SIZE,
			 "Could not evaluate expression \"%s\" \n", input);
		return err;
	}

	double value = root->value;

	cevil_expr_free(&expr);

	*result = value;

	return cevil_ok;
}

#endif // CEVIL_IMPLEMENTATION
