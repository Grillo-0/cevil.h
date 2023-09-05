#ifndef CEVIL_H_
#define CEVIL_H_

#define CEVIL_ERROR_BUF_SIZE 256

enum cevil_error_type {
	CEVIL_ERROK,
	CEVIL_ERRPAR,
	CEVIL_ERRAST,
	CEVIL_ERREVAL,
	CEVIL_ERRGEN,
};

struct cevil_error {
	enum cevil_error_type type;
	char msg[CEVIL_ERROR_BUF_SIZE];
};

struct cevil_error cevil_eval(const char *expr, double *result);
void cevil_print_error(struct cevil_error *err);

#endif // CEVIL_H_

#ifdef CEVIL_IMPLEMENTATION

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define cevil_ok (struct cevil_error){.type = CEVIL_ERROK}

#define SV_LITERAL(str) (struct sv){.base = str, .len = sizeof(str) - 1}
#define SV_FMT "%.*s"
#define SV_ARGS(sv) sv.len, sv.base

struct sv {
	const char *base;
	size_t len;
};

struct sv_stg {
	struct sv *arr;
	size_t len;
	size_t capacity;
};

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
		nodeid_t lhs;
		nodeid_t rhs;
	};
} cevil_as;

struct cevil_node {
	struct sv src;
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
	const char *src;
	nodeid_t root;
	struct node_storage stg;
};

static struct cevil_error error_create(enum cevil_error_type type,
				      const char *fmt, ...) {
	struct cevil_error err = {.type = type};

	va_list args;
	va_start(args, fmt);

	vsnprintf(err.msg, CEVIL_ERROR_BUF_SIZE, fmt, args);

	va_end(args);

	return err;
}

void cevil_print_error(struct cevil_error *err) {
	switch (err->type) {
	case CEVIL_ERROK:
		fprintf(stderr, "No error\n");
		break;
	case CEVIL_ERRPAR:
		fprintf(stderr, "Parser error:");
		break;
	case CEVIL_ERRAST:
		fprintf(stderr, "AST error:");
		break;
	case CEVIL_ERREVAL:
		fprintf(stderr, "Eval error:");
		break;
	case CEVIL_ERRGEN:
		fprintf(stderr, "Generic error:");
		break;
	}

	fprintf(stderr, " %.*s\n", CEVIL_ERROR_BUF_SIZE - 1, err->msg);
}

static bool sv_eq(const struct sv a, const struct sv b) {
	if (a.len != b.len)
		return false;

	return strncmp(a.base, b.base, a.len) == 0;
}

static void sv_stg_add(struct sv_stg *stg, struct sv tk) {
	stg->len++;

	if (stg->len > stg->capacity) {
		if (stg->capacity == 0)
			stg->capacity = 128;

		stg->capacity *= 2;
		stg->arr = realloc(stg->arr, sizeof(*stg) * stg->capacity);
		assert(stg->arr != NULL);
	}

	stg->arr[stg->len - 1] = tk;
}

static struct cevil_node* node_storage_get(struct node_storage stg,
					   nodeid_t index) {
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

	expr->src = ptr;
}

static void cevil_expr_free(struct cevil_expr *expr) {
	free((char*)expr->src);
	node_storage_free(&expr->stg);
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

static const char *whitespace = " \t\n\r";
static const char *delimeters = " \t\n\r+-/*()";

static struct sv next_tk(struct sv *expr) {
	size_t offset = strspn(expr->base, whitespace);
	expr->base += offset;
	expr->len -= offset;

	size_t tk_len = strcspn(expr->base, delimeters);

	if (tk_len == 0)
		tk_len++;

	struct sv tk = {
		.base = expr->base,
		.len = tk_len,
	};

	expr->base += tk_len;
	expr->len -= tk_len;

	return tk;
}

static struct sv_stg lex(const char *expr_str) {
	struct sv_stg stg = {0};

	struct sv expr = {
		.base = expr_str,
		.len = strlen(expr_str),
	};

	while (expr.len != 0) {
		struct sv tk = next_tk(&expr);
		if (*tk.base == '\0')
			break;
		sv_stg_add(&stg, tk);
	}

	return stg;
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

static struct cevil_error tk_to_node(struct sv tk, struct cevil_node *node) {
	node->src = tk;

	char *end;
	double value = strtod(tk.base, &end);

	if (end == tk.base + tk.len) {
		node->type = CEVIL_NUM_NODE;
		node->as.number = value;
	} else if (sv_eq(tk, SV_LITERAL("+"))) {
		node->type = CEVIL_PLUS_NODE;
	} else if (sv_eq(tk, SV_LITERAL("-"))) {
		node->type = CEVIL_MINUS_NODE;
	} else if (sv_eq(tk, SV_LITERAL("*"))) {
		node->type = CEVIL_MULT_NODE;
	} else if (sv_eq(tk, SV_LITERAL("/"))) {
		node->type = CEVIL_DIV_NODE;
	} else {
		return error_create(CEVIL_ERRPAR,
				    "token \"" SV_FMT
				    "\" did not match any node type",
				    SV_ARGS(tk));
	}

	return cevil_ok;
}

static struct cevil_error build_ast_rec(struct cevil_expr *expr,
					nodeid_t *root) {
	nodeid_t next_id = *root + 1;

	if (next_id >= expr->stg.len)
		return cevil_ok;

	struct cevil_node *next_node = node_storage_get(expr->stg, next_id);
	struct cevil_node *root_node = node_storage_get(expr->stg, *root);

	if (is_binary_op(next_node->type)) {
		struct cevil_error err;

		next_node->as.lhs = *root;
		next_node->as.rhs = next_id;
		err = build_ast_rec(expr, &next_node->as.rhs);

		struct cevil_node *rhs = node_storage_get(expr->stg,
							  next_node->as.rhs);
		// Switch nodes by the precedence
		//
		// Ex: 2 * 14 + 2
		//
		//   *          +
		//  / \        / \
		// 2   +  ->  *  2
		//    / \    / \
		//   14  2  2  14
		if (is_binary_op(rhs->type) &&
		    precedence(next_node->type) >= precedence(rhs->type)) {
			nodeid_t new_root = next_node->as.rhs;
			next_node->as.rhs = rhs->as.lhs;
			rhs->as.lhs = next_id;
			next_id = new_root;
		}

		*root = next_id;

		if (err.type != CEVIL_ERROK)
			return err;
	} else {
		if (!is_binary_op(root_node->type))
			return error_create(CEVIL_ERRAST,
					    "number after another is not a "
					    "valid expression");
		*root = next_id;
		return build_ast_rec(expr, root);
	}

	return cevil_ok;
}

static struct cevil_error build_ast(struct cevil_expr *expr) {
	if (is_binary_op(expr->stg.arr[0].type))
		return error_create(CEVIL_ERRAST,
				    "binary operation can't be on the "
				    "start of an expression");

	expr->root = 0;
	return build_ast_rec(expr, &expr->root);
}

static struct cevil_error parse(struct cevil_expr *expr) {
	struct sv_stg tks = lex(expr->src);

	for (size_t i = 0; i < tks.len; i++) {
		struct cevil_error err;
		nodeid_t id = node_storage_alloc(&expr->stg);
		struct cevil_node *node = node_storage_get(expr->stg, id);
		err = tk_to_node(tks.arr[i], node);

		if (err.type != CEVIL_ERROK)
			return err;
	}

	free(tks.arr);

	return build_ast(expr);
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
	struct cevil_error err;
	struct cevil_expr expr;
	cevil_expr_init(&expr, input);

	err = parse(&expr);

	if (err.type != CEVIL_ERROK)
		return err;

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
