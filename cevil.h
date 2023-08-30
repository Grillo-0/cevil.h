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

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define cevil_ok (struct cevil_error){.type = CEVIL_ERROK}

typedef size_t nodeid_t;

enum cevil_node_type {
	CEVIL_NUM_NODE,
	CEVIL_PLUS_NODE,
	CEVIL_MINUS_NODE,
	CEVIL_MULT_NODE,
	CEVIL_DIV_NODE,
	CEVIL_NODE_SIZE,
};

typedef union {
	double number;
	struct {
		nodeid_t rhs;
		nodeid_t lhs;
	};
} cevil_as;

struct cevil_node {
	enum cevil_node_type type;
	cevil_as as;
	bool evaluated;
	double value;
};

struct node_storage {
	struct cevil_node *arr;
	size_t len;
	size_t capacity;
};

struct cevil_expr {
	const char *base;
	const char *parser_cursor;
	nodeid_t root;
	struct node_storage stg;
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

static nodeid_t node_storage_alloc(struct node_storage *stg) {
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

static struct cevil_node* node_storage_get(struct node_storage stg, nodeid_t index) {
	assert(index < stg.len);

	return &stg.arr[index];
}

static void node_storage_free(struct node_storage *stg) {
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
	node_storage_free(&expr->stg);
}

void chop(const char **str) {
	while(**str != '\0' && isspace(**str))
		(*str)++;
}

static bool is_binary_op(enum cevil_node_type node) {
	static bool is_binary_op_table[CEVIL_NODE_SIZE] = {
		[CEVIL_PLUS_NODE] = true,
		[CEVIL_MINUS_NODE] = true,
		[CEVIL_MULT_NODE] = true,
		[CEVIL_DIV_NODE] = true,
	};

	return is_binary_op_table[node];
}

/**
 * Get the precedence value of an operation
 * @node: token to the precedence value
 *
 * Returns:
 *
 * Returns the precedence value or 0 if the token is not a operation.
 * The bigger the number, the early the operation needs to be
 * evaluated.
 */
static size_t precedence(enum cevil_node_type node) {
	static size_t precedence_table[CEVIL_NODE_SIZE] = {
		[CEVIL_PLUS_NODE] = 1,
		[CEVIL_MINUS_NODE] = 1,
		[CEVIL_MULT_NODE] = 2,
		[CEVIL_DIV_NODE] = 2,
	};

	return precedence_table[node];
}

struct cevil_error add_node_to_ast(struct cevil_expr *expr, nodeid_t *node_id) {
	*node_id = node_storage_alloc(&expr->stg);
	struct cevil_node *node = node_storage_get(expr->stg, *node_id);
	node->evaluated = false;

	char* end;

	double value = strtod(expr->parser_cursor, &end);

	if (end != expr->parser_cursor) {
		node->type = CEVIL_NUM_NODE;
		node->as.number = value;
		expr->parser_cursor = end;
	} else if (*expr->parser_cursor == '+') {
		node->type = CEVIL_PLUS_NODE;
		expr->parser_cursor++;
	} else if (*expr->parser_cursor == '-') {
		node->type = CEVIL_MINUS_NODE;
		expr->parser_cursor++;
	} else if (*expr->parser_cursor == '*') {
		node->type = CEVIL_MULT_NODE;
		expr->parser_cursor++;
	} else if (*expr->parser_cursor == '/') {
		node->type = CEVIL_DIV_NODE;
		expr->parser_cursor++;
	} else {
		struct cevil_error err = {.type = CEVIL_ERRPAR};
		snprintf(err.msg, CEVIL_ERROR_BUF_SIZE,
			 "error: Unexpected charcter '%c'\n", *expr->base);
		return err;
	}

	if (is_binary_op(node->type)) {
		struct cevil_error err;

		err = add_node_to_ast(expr, &node->as.rhs);

		struct cevil_node *root = node_storage_get(expr->stg, expr->root);

		if (is_binary_op(root->type) &&
		    precedence(node->type) > precedence(root->type)) {
			node->as.lhs = root->as.rhs;
			root->as.rhs = *node_id;
			*node_id = expr->root;
		} else {
			node->as.lhs = expr->root;
		}

		if (err.type != CEVIL_ERROK)
			return err;
	};

	return cevil_ok;
}

void eval(nodeid_t root_id, struct node_storage stg) {
	struct cevil_node *root = node_storage_get(stg, root_id);

	struct cevil_node *rhs = NULL;
	struct cevil_node *lhs = NULL;

	switch (root->type) {
	case CEVIL_NUM_NODE:
		root->evaluated = true;
		root->value = root->as.number;
		break;
	case CEVIL_PLUS_NODE:
		rhs = node_storage_get(stg, root->as.rhs);
		lhs = node_storage_get(stg, root->as.lhs);

		eval(root->as.rhs, stg);
		eval(root->as.lhs, stg);

		root->evaluated = lhs->evaluated && rhs->evaluated;

		if (!root->evaluated)
			return;

		root->value = lhs->value + rhs->value;
		break;
	case CEVIL_MINUS_NODE:
		rhs = node_storage_get(stg, root->as.rhs);
		lhs = node_storage_get(stg, root->as.lhs);

		eval(root->as.rhs, stg);
		eval(root->as.lhs, stg);

		root->evaluated = lhs->evaluated && rhs->evaluated;

		if (!root->evaluated)
			return;

		root->value = lhs->value - rhs->value;
		break;
	case CEVIL_MULT_NODE:
		rhs = node_storage_get(stg, root->as.rhs);
		lhs = node_storage_get(stg, root->as.lhs);

		eval(root->as.rhs, stg);
		eval(root->as.lhs, stg);

		root->evaluated = lhs->evaluated && rhs->evaluated;

		if (!root->evaluated)
			return;

		root->value = lhs->value * rhs->value;
		break;
	case CEVIL_DIV_NODE:
		rhs = node_storage_get(stg, root->as.rhs);
		lhs = node_storage_get(stg, root->as.lhs);

		eval(root->as.rhs, stg);
		eval(root->as.lhs, stg);

		root->evaluated = lhs->evaluated && rhs->evaluated;

		if (!root->evaluated)
			return;

		root->value = lhs->value / rhs->value;
		break;
	case CEVIL_NODE_SIZE:
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
		nodeid_t result;
		err = add_node_to_ast(&expr, &result);
		expr.root = result;
		if (err.type != CEVIL_ERROK)
			return err;
		chop(&expr.parser_cursor);
	}

	eval(expr.root, expr.stg);

	struct cevil_node *root = node_storage_get(expr.stg, expr.root);

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
