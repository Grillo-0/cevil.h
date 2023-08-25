#ifndef CEVIL_H_
#define CEVIL_H_

double cevil_eval(const char *expr);

#endif // CEVIL_H_

#ifdef CEVIL_IMPLEMENTATION

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef size_t tkid_t;

enum cevil_tk_type {
	CEVIL_NUM_TK,
	CEVIL_PLUS_TK,
	CEVIL_MINUS_TK,
	CEVIL_MULT_TK,
	CEVIL_DIV_TK,
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

tkid_t next_tk(struct cevil_expr *expr) {
	tkid_t tk_id = tk_storage_alloc(&expr->stg);
	struct cevil_tk *tk = tk_storage_get(expr->stg, tk_id);
	tk->evaluated = false;

	char* end;

	double value = strtod(expr->parser_cursor, &end);

	if (end != expr->parser_cursor) {
		tk->type = CEVIL_NUM_TK;
		tk->as.number = value;
		expr->parser_cursor = end;
	} else if (*expr->parser_cursor == '+') {
		tk->type = CEVIL_PLUS_TK;
		tk->as.lhs = expr->root;
		expr->parser_cursor++;
		tk->as.rhs = next_tk(expr);
	} else if (*expr->parser_cursor == '-') {
		tk->type = CEVIL_MINUS_TK;
		tk->as.lhs = expr->root;
		expr->parser_cursor++;
		tk->as.rhs = next_tk(expr);
	} else if (*expr->parser_cursor == '*') {
		tk->type = CEVIL_MULT_TK;
		tk->as.lhs = expr->root;
		expr->parser_cursor++;
		tk->as.rhs = next_tk(expr);
	} else if (*expr->parser_cursor == '/') {
		tk->type = CEVIL_DIV_TK;
		tk->as.lhs = expr->root;
		expr->parser_cursor++;
		tk->as.rhs = next_tk(expr);
	} else {
		fprintf(stderr, "error: Unexpected charcter '%c'\n", *expr->base);
		exit(-1);
	}

	return tk_id;
}

void eval(tkid_t root_id, struct tk_storage stg) {
	struct cevil_tk *root = tk_storage_get(stg, root_id);

	struct cevil_tk *rhs = NULL;
	struct cevil_tk *lhs = NULL;

	switch(root->type) {
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
	default:
		assert(false && "Unreachable");
		break;
	}
}

double cevil_eval(const char *input) {
	struct cevil_expr expr;
	cevil_expr_init(&expr, input);

	chop(&expr.parser_cursor);
	while (*expr.parser_cursor != '\0') {
		expr.root = next_tk(&expr);
		chop(&expr.parser_cursor);
	}

	eval(expr.root, expr.stg);

	struct cevil_tk *root = tk_storage_get(expr.stg, expr.root);

	if (root->evaluated == false) {
		fprintf(stderr, "Could not evaluate expression \"%s\" \n",
			input);
		exit(-1);
	}

	double value = root->value;

	cevil_expr_free(&expr);

	return value;
}

#endif // CEVIL_IMPLEMENTATION
