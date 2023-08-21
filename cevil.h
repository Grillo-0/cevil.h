#ifndef CEVIL_H_
#define CEVIL_H_

double cevil_eval(char *expr);

#endif // CEVIL_H_

#ifdef CEVIL_IMPLEMENTATION

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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
		struct cevil_tk *rhs;
		struct cevil_tk *lhs;
	};
} cevil_as;

typedef struct cevil_tk {
	enum cevil_tk_type type;
	cevil_as as;
	bool evaluated;
	double value;
} cevil_tk;

typedef struct {
	char* base;
	struct cevil_tk *root;
} cevil_expr;

void chop(char **str) {
	while(**str != '\0' && isspace(**str))
		(*str)++;
}

cevil_tk* next_tk(cevil_expr *expr) {

	cevil_tk *tk = malloc(sizeof(*tk));

	char* end = expr->base + strlen(expr->base);

	double value = strtod(expr->base, &end);

	if (end != expr->base) {
		tk->type = CEVIL_NUM_TK;
		tk->as.number = value;
		expr->base = end;
	} else if (*expr->base == '+') {
		tk->type = CEVIL_PLUS_TK;
		tk->as.rhs = expr->root;
		expr->base++;
		tk->as.lhs = next_tk(expr);
	} else if (*expr->base == '-') {
		tk->type = CEVIL_MINUS_TK;
		tk->as.rhs = expr->root;
		expr->base++;
		tk->as.lhs = next_tk(expr);
	} else if (*expr->base == '*') {
		tk->type = CEVIL_MULT_TK;
		tk->as.rhs = expr->root;
		expr->base++;
		tk->as.lhs = next_tk(expr);
	} else if (*expr->base == '/') {
		tk->type = CEVIL_DIV_TK;
		tk->as.rhs = expr->root;
		expr->base++;
		tk->as.lhs = next_tk(expr);
	} else {
		fprintf(stderr, "unexpected charcter '%c'\n", *expr->base);
		exit(-1);
	}

	return tk;
}

void eval(cevil_tk *root) {
	if (root == NULL)
		return;

	switch(root->type) {
	case CEVIL_NUM_TK:
		root->evaluated = true;
		root->value = root->as.number;
		break;
	case CEVIL_PLUS_TK:
		eval(root->as.rhs);
		eval(root->as.lhs);
		root->evaluated = root->as.rhs && root->as.lhs;

		if (!root->evaluated)
			return;

		root->value = root->as.rhs->value + root->as.lhs->value;
		break;
	case CEVIL_MINUS_TK:
		eval(root->as.rhs);
		eval(root->as.lhs);
		root->evaluated = root->as.rhs && root->as.lhs;

		if (!root->evaluated)
			return;

		root->value = root->as.rhs->value - root->as.lhs->value;
		break;
	case CEVIL_MULT_TK:
		eval(root->as.rhs);
		eval(root->as.lhs);
		root->evaluated = root->as.rhs && root->as.lhs;

		if (!root->evaluated)
			return;

		root->value = root->as.rhs->value * root->as.lhs->value;
		break;
	case CEVIL_DIV_TK:
		eval(root->as.rhs);
		eval(root->as.lhs);
		root->evaluated = root->as.rhs && root->as.lhs;

		if (!root->evaluated)
			return;

		root->value = root->as.rhs->value / root->as.lhs->value;
		break;
	default:
		assert(false && "Unreachable");
		break;
	}
}

double cevil_eval(char *input) {
	cevil_expr expr;

	expr.base = calloc(sizeof(*input), strlen(input) + 1);
	memcpy(expr.base, input, strlen(input) * sizeof(*input));

	chop(&expr.base);
	while (*expr.base) {
		expr.root = next_tk(&expr);
		chop(&expr.base);
	}

	eval(expr.root);

	if (expr.root->evaluated == false) {
		fprintf(stderr, "Could not evaluate expression \"%s\" \n",
			input);
		exit(-1);
	}

	return expr.root->value;
}

#endif // CEVIL_IMPLEMENTATION
