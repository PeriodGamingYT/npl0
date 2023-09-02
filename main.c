#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// tokens
enum { 
	NUM, 
	STOP, 
	EQ, 
	MORE_EQ, 
	LESS_EQ, 
	NOT_EQ, 
	SHL, 
	SHR, 
	IDENT, 
	B0,
	B8,
	B16,
	B32,
	B64,
	BPTR,
	IF,
	LOOP,
	LINK,
	STACK,
	HEAP,
	NOFREE,
	RODATA,
	SIZEOF,
	ELSE
};

// macros
#define H(_x) \
	fprintf(stderr, "hit%s\n", #_x);

#define IS_IDENT_START(_x) \
	((_x >= 'A' && _x <= 'Z') || (_x >= 'a' && _x <= 'z') || (_x == '_'))

#define IS_NUMBER(_x) \
	(_x >= '0' && _x <= '9')

#define IS_STOP(_x) \
	(!(_x) || _x == -1)

#define ARRAY_SIZE(_x) \
	((int)(sizeof(_x) / sizeof((_x)[0])))

#define IS_WHITESPACE(_x) \
	((_x) == ' ' || (_x) == '\t' || (_x) == '\n' || (_x) == '\r')

#define IS_NEWLINE(_x) \
	((_x) == '\n' || (_x) == '\r')

#define ARRAY_PUSH_UNDEF(_x, _y) \
	_x##_size++; \
	_x = safe_realloc(_x, sizeof(_y) * _x##_size)

#define ARRAY_PUSH(_x, _y, _z) \
	ARRAY_PUSH_UNDEF(_x, _y); \
	_x[_x##_size - 1] = _z

#define NULL_RETURN_REF(_x) \
	if(_x == NULL || *_x == NULL) { \
		return; \
	}

#define NOT_NULL_FREE(_x) \
	if(_x != NULL) { \
		free(_x); \
		_x = NULL; \
	}

#define NOT_NULL_FREE_SIZE(_x) \
	NOT_NULL_FREE(_x); \
	_x##_size = 0;

// globals
typedef intptr_t value_t;
int token = 0;
value_t token_val = 0;
char *src = NULL;
char *start_src = NULL;
char *buffer_ptr = NULL;

// lexing
// 32 (actually 31 because of null term) is plenty, if you need more than that, you're screwed anyway
#define IDENT_MAX 32
char ident[IDENT_MAX] = { 0 };
int ident_index = 0;
void next() {

	// whitespace
	if(IS_STOP(*src)) {
		token = STOP;
		return;
	}

	while(IS_WHITESPACE(*src)) {
		src++;
	}

	if(IS_STOP(*src)) {
		token = STOP;
		return;
	}

	// comments
	if(*src == '\\') {
		src++;
		while(IS_NEWLINE(*src) && !IS_STOP(*src)) {
			src++;
		}

		while(IS_WHITESPACE(*src)) {
			src++;
		}

		if(IS_STOP(*src)) {
			token = STOP;
			return;
		}
	}

	// integers (negatives handled by value())
	if(IS_NUMBER(*src)) {
		token_val = *src - '0';
		token = NUM;
		src++;
		while(IS_NUMBER(*src)) {
			token_val = (token_val * 10) + (*src - '0');
			src++;
		}

		return;
	}

	// idents
	if(IS_IDENT_START(*src)) {
		ident_index = 0;
		memset(ident, 0, sizeof(char) * IDENT_MAX);
		do {
			ident[ident_index++] = *src++;
			if(ident_index > IDENT_MAX - 2) {
				fprintf(stderr, "name too large\n");
				exit(1);
			}
		} while(IS_IDENT_START(*src) || IS_NUMBER(*src));

		token = IDENT;

		// reserved
		const int new_reserved[] = { 
			B0, 
			B8, 
			B16, 
			B32, 
			B64,
			BPTR,
			IF, 
			LOOP, 
			LINK, 
			STACK, 
			HEAP, 
			NOFREE,
			SIZEOF,
			ELSE
		};

		const char *reserved[] = {
			"b0",
			"b8",
			"b16",
			"b32",
			"b64",
			"bptr",
			"if",
			"loop",
			"link",
			"stack",
			"heap",
			"nofree",
			"sizeof",
			"else"
		};

		for(int i = 0; i < ARRAY_SIZE(reserved); i++) {
			if(!strcmp(reserved[i], ident)) {
				memset(ident, 0, sizeof(char) * IDENT_MAX);
				token = new_reserved[i];
				return;
			}
		}
		
		return;
	}
	
	// opers
	const int new_token[] = { 
		EQ, 
		MORE_EQ, 
		LESS_EQ, 
		NOT_EQ, 
		SHL, 
		SHR 
	};
	
	const char *two_char_opers[] = {
		"=><!<>",
		"====<>"
	};
	
	for(int i = 0; two_char_opers[0][i] != 0; i++) {
		if(*src == two_char_opers[0][i] && src[1] == two_char_opers[1][i]) {
			token = new_token[i];
			src += 2;
			return;
		}
	}

	token = *(src++);
}

int expect(char x) {
	if(token != x) {
		fprintf(stderr, "expected %c:%d, but got %c:%d instead\n", x, x, token, token);
		exit(1);
	}

	next();
}

// memory alloc
void *safe_realloc(void *ptr, unsigned int size) {
	if(size <= 0) {
		free(ptr);
		return NULL;
	}
	
	void *temp = realloc(ptr, size);
	if(temp == NULL && size > 0) {
		if(ptr != NULL) {
			free(ptr);
		}
		
		fprintf(stderr, "failed realloc %p -> %p, %d\n", ptr, temp, size);
		exit(1);
	}

	return temp;
}

void *safe_malloc(unsigned int size) {
	if(size <= 0) {
		return NULL;
	}
	
	void *temp = malloc(size);
	if(temp == NULL && size > 0) {
		fprintf(stderr, "failed malloc\n");
		exit(1);
	}

	return temp;
}

// structs
typedef struct {
	int size;
	int *is_pos;
	int *type_size;
	char **name;
	char *struct_name;
} struct_def_t;

void print_struct_def(struct_def_t *struct_def) {
	if(struct_def == NULL) {
		fprintf(stderr, "struct_def is null\n");
		return;
	}
	
	fprintf(stderr, "struct_def\n");
	fprintf(stderr, "size %d\n", struct_def->size);
	fprintf(stderr, "struct_name %s\n", struct_def->struct_name);
	for(int i = 0; i < struct_def->size; i++) {
		fprintf(stderr, "is_pos %d: %d\n", i, struct_def->is_pos[i]);
		fprintf(stderr, "type_size %d: %d\n", i, struct_def->type_size[i]);
		fprintf(stderr, "name %d: %s\n", i, struct_def->name[i]);
	}

	fprintf(stderr, "\n");
}

// part of vars
#define VALUE_MAX 8
typedef unsigned char var_value_t[VALUE_MAX];
typedef struct {
	struct_def_t *struct_def;
	var_value_t *vals;
	char *name;
} struct_val_t;

void print_struct_val(struct_val_t *struct_val) {
	if(struct_val == NULL) {
		fprintf(stderr, "struct_val is null\n\n");
		return;
	}
	
	fprintf(stderr, "struct_val\n");
	print_struct_def(struct_val->struct_def);
	if(struct_val->struct_def == NULL) {
		fprintf(stderr, "struct_def in struct_val is null\n\n");
	}
	
	for(int i = 0; struct_val->struct_def != NULL && i < struct_val->struct_def->size; i++) {
		fprintf(stderr, "vals %d: %ld\n", i, (value_t)(struct_val->vals[i]));
	}

	fprintf(stderr, "name %s\n\n", struct_val->name);
}

void free_struct_vals(struct_val_t **struct_val) {
	NULL_RETURN_REF(struct_val);
	NOT_NULL_FREE((*struct_val)->vals);
	NOT_NULL_FREE((*struct_val)->name);
	NOT_NULL_FREE(*struct_val);
}

int struct_val_prop(struct_val_t *struct_val, char *name) {
	for(int i = 0; i < struct_val->struct_def->size; i++) {
		if(!strcmp(struct_val->struct_def->name[i], name)) {
			return i;
		}
	}

	return -1;
}

struct_val_t *make_struct_val(struct_def_t *struct_def, char *name) {
	if(struct_def == NULL) {
		fprintf(stderr, "make_struct_val(), struct_def arg is null\n");
		exit(1);
	}
	
	struct_val_t *struct_val = safe_malloc(sizeof(struct_val_t));
	struct_val->struct_def = struct_def;
	struct_val->vals = safe_malloc(sizeof(var_value_t) * struct_def->size);
	memset(struct_val->vals, 0, sizeof(var_value_t) * struct_def->size);
	int name_length = strlen(name) + 1;
	struct_val->name = safe_malloc(name_length);
	memcpy(struct_val->name, name, name_length - 1);
	struct_val->name[name_length - 1] = 0;
	return struct_val;
}

void free_struct_val(struct_val_t **struct_val) {
	NULL_RETURN_REF(struct_val);
	NOT_NULL_FREE((*struct_val)->vals);
	NOT_NULL_FREE((*struct_val)->name);
	NOT_NULL_FREE(*struct_val);
}

struct_def_t **struct_stack = NULL;
int struct_stack_size = 0;
int index_struct_def(char *name) {
	for(int i = 0; i < struct_stack_size; i++) {
		if(!strcmp(struct_stack[i]->struct_name, name)) {
			return i;
		}
	}

	return -1;
}

struct_def_t *make_struct_def() {
	struct_def_t *result = safe_malloc(sizeof(struct_def_t));
	*result = (struct_def_t) {
		0,
		NULL,
		NULL,
		NULL,
		NULL
	};
	
	return result;
}

void free_struct_def(struct_def_t **struct_def) {
	NULL_RETURN_REF(struct_def);
	NOT_NULL_FREE((*struct_def)->is_pos);
	NOT_NULL_FREE((*struct_def)->type_size);
	if((*struct_def)->name != NULL) {
		for(int i = 0; i < (*struct_def)->size; i++) {
			NOT_NULL_FREE((*struct_def)->name[i]);
		}

		free((*struct_def)->name);
		(*struct_def)->name = NULL;
	}

	NOT_NULL_FREE((*struct_def)->struct_name);
	(*struct_def)->size = 0;
	NOT_NULL_FREE(*struct_def);
}

// other part of vars
typedef struct var_s {
	char *name;
	int type_size;
	int is_pos;
	var_value_t *value;
	struct_val_t *struct_val;
} var_t;

void add_struct_def(struct_def_t *struct_def, var_t var) {
	struct_def->size++;
	struct_def->is_pos = safe_realloc(struct_def->is_pos, sizeof(int) * struct_def->size);
	struct_def->type_size = safe_realloc(struct_def->type_size, sizeof(int) * struct_def->size);
	struct_def->name = safe_realloc(struct_def->name, sizeof(char *) * struct_def->size);	
	struct_def->is_pos[struct_def->size - 1] = var.is_pos;
	struct_def->type_size[struct_def->size - 1] = var.type_size;
	int name_size = strlen(var.name);
	struct_def->name[struct_def->size - 1] = safe_malloc(name_size + 1);
	memcpy(struct_def->name[struct_def->size - 1], var.name, name_size);
	struct_def->name[struct_def->size - 1][name_size] = 0;
}

// rest of vars
var_value_t *var_values = NULL;
int var_values_size = 0;
var_value_t *var_values_add() {
	ARRAY_PUSH_UNDEF(var_values, var_value_t);
	memset(var_values[var_values_size - 1], 0, sizeof(var_value_t));
	return &(var_values[var_values_size - 1]);
}

void print_value_bytes(const char *message, value_t value) {
	fprintf(stderr, "value %s\n", message);
	unsigned char *value_ptr = (unsigned char *) &value;
	for(int i = 0; i < (int) sizeof(value_t); i++) {
		fprintf(stderr, "\t%d: %d\n", i, (unsigned char) value_ptr[i]);
	}

	fprintf(stderr, "\n");
}

#define PRINT_VALUE_BYTES(_x) \
	print_value_bytes(#_x, _x)

value_t var_to_int(var_t var) {
	if(var.value == NULL) {

		// this is a struct, don't handle idents here.
		return 0;
	}
	
	value_t result = *((value_t *) var.value);
	value_t min_two_comp = 1 << ((var.type_size * 8) - 1);

	// at the end of min_two_comp, after the signed digit, it's filled with ones
	// this makes it impossible to check if something is truly negative because
	// it has a value var can never reach
	// the code below fixes this problem
	unsigned char *min_comp_ptr = (unsigned char *) &min_two_comp;
	for(int i = var.type_size; i < (int) sizeof(value_t); i++) {
		min_comp_ptr[i] = 0;
	}

	// numbers like b32/b64 can be negative even if unsigned
	// this code handles that edge case
	int is_val_comp = result >= min_two_comp && min_two_comp > 0;
	int is_neg = !var.is_pos && (is_val_comp || result < 0);
	unsigned char *result_ptr = (unsigned char *)(&result);

	// make remaining bits 0xff to turn n-bit unsigned int to 64/32 bit
	// unsigned int
	for(int i = var.type_size; i < (int) sizeof(value_t); i++) {
		result_ptr[i] = 0xff * is_neg;
	}

	return result;
}

// (*(var->value))[i] parens are needed
void assign_int_var(var_t *var, value_t x) {
	if(var->value == NULL) {
		var->value = var_values_add();
	}
	
	memset(*(var->value), 0, sizeof(var_value_t));
	for(int i = 0; i < var->type_size; i++) {
		(*(var->value))[i] = (unsigned char)((x >> (i * 8)) & 0xff);
	}
}

var_t *vars = NULL;
int vars_size = 0;
int *var_scopes = NULL;
int var_scopes_size = 0;
void ident_var_add() {
	ARRAY_PUSH_UNDEF(vars, var_t);
	int ident_size = strlen(ident);
	vars[vars_size - 1].name = safe_malloc(sizeof(char) * (ident_size + 1));
	memcpy(vars[vars_size - 1].name, ident, ident_size);
	vars[vars_size - 1].name[ident_size] = 0;
	var_scopes[var_scopes_size - 1]++;
}

int ident_var_index(int is_last_error) {
	if(ident == NULL) {
		fprintf(stderr, "ident_var_index stopped program because ident was null\n");
		exit(1);
	}

	for(int i = 0; i < vars_size; i++) {
		if(vars[i].name == NULL) {
			continue;
		}
		
		if(!strcmp(vars[i].name, ident)) {
			return i;
		}
	}

	if(is_last_error) {
		fprintf(stderr, "nonexistent variable %s\n", ident);
		exit(1);
	}
	
	ident_var_add();
	return vars_size - 1;
}

void var_scope_add() {
	ARRAY_PUSH(var_scopes, int, 0);
}

void var_scope_remove() {
	if(var_scopes_size <= 0) {
		fprintf(stderr, "can't remove scope any further\n");
		exit(1);
	}
	
	int size = vars_size;
	int scope_size = var_scopes[var_scopes_size - 1];
	for(int i = size - 1; i >= size - scope_size && i >= 0; i--) {
		if(vars[i].name == NULL) {
			continue;
		}
		
		free(vars[i].name);
		vars[i].name = NULL;
	}

	vars_size -= scope_size;
	vars = safe_realloc(vars, sizeof(var_t) * vars_size);
	var_scopes_size--;
	var_scopes = safe_realloc(var_scopes, sizeof(int) * var_scopes_size);
}

typedef struct {
	var_value_t *value;
	int type_size;
	int is_pos;
} var_def_t;

// expr parsing
int int_pow(int base, int exp) {
	if(exp == 0) {
		return 1;
	}
	
	int orig_base = base;

	// pre-dec oper because post-dec exp result is off by one multiplication
	while(--exp > 0) {
		base *= orig_base;
	}

	return base;
}

char ident_copy[IDENT_MAX] = { 0 };
value_t expr();
int found_ident = 0;
struct_val_t *current_struct_val = NULL;
var_def_t struct_prop_def = { NULL, 0, 0 };
value_t value() {
	value_t eval_value = 0;
	if(token == STOP || !(*src) || *src == -1) {
		fprintf(stderr, "can't find a token to get value of, found end of file instead\n");
		exit(1);
	}

	switch(token) {
		case '(':
			expect('(');
			eval_value = expr();
			expect(')');
			break;

		case '!': expect('!'); eval_value = !expr(); break;
		case '~': expect('~'); eval_value = ~expr(); break;
		case '-': expect('-'); eval_value = -expr(); break;
		case ':':
			expect(':');
			eval_value = (value_t) vars[ident_var_index(1)].value;
			expect(IDENT);
			break;

		case '@':
			expect('@');
			if(token < B8 || token > BPTR) {
				fprintf(stderr, "when using hashtag, provide a type and then a +/-\n");
				exit(1);
			}

			int hash_size = token != BPTR
				? int_pow(2, token - B8)
				: (int) sizeof(value_t);

			next();
			if(token != '+' && token != '-') {
				fprintf(stderr, "you forgot to provide a +/- when using a hashtag\n");
				exit(1);
			}

			int hash_pos = token == '+';
			next();
			unsigned char *address = (unsigned char *) expr();
			value_t val = 0;
			for(int i = 0; i < hash_size; i++) {
				val |= address[i] << (i * 8);
			}

			value_t min_two_comp = 1 << ((hash_size * 8) - 1);

			// numbers like b32/b64 can be negative even if unsigned
			// this code handles that edge case
			int is_val_comp = val >= min_two_comp && min_two_comp > 0;
			int is_neg = !hash_pos && (is_val_comp || val < 0);
			unsigned char *val_ptr = (unsigned char *) &val;

			// make remaining bits 0xff to turn n-bit unsigned int to 64/32 bit
			// unsigned int.
			for(int i = hash_size; i < (int) sizeof(value_t); i++) {
				val_ptr[i] = 0xff * is_neg;
			}

			eval_value = val;
			break;
		
		case IDENT:
			if(current_struct_val != NULL) {
				memcpy(ident_copy, ident, IDENT_MAX);
				expect(IDENT);
				int prop_index = struct_val_prop(current_struct_val, ident_copy);
				if(prop_index == -1) {
					print_struct_val(current_struct_val);
					fprintf(stderr, "tried to access nonexistant structure property\n");
					exit(1);
				}

				if(token != '.') {
					var_t var = {
						NULL,
						current_struct_val->struct_def->type_size[prop_index],
						current_struct_val->struct_def->is_pos[prop_index],
						(unsigned char (*)[8])(current_struct_val->vals[prop_index]),
						NULL
					};

					eval_value = var_to_int(var);
					struct_prop_def = (var_def_t) {
						var.value,
						var.type_size,
						var.is_pos
					};

					current_struct_val = NULL;
					break;
				}

				current_struct_val = (struct_val_t *) current_struct_val->vals[prop_index];
				value();
				break;
			}
						
			int index = ident_var_index(1);
			var_t var = vars[index];
			eval_value = var_to_int(var);
			memcpy(ident_copy, ident, IDENT_MAX);
			expect(IDENT);
			found_ident = 1;
			if(token == '.' && var.struct_val != NULL) {
				current_struct_val = var.struct_val;
				next();
				value();
			} else if(token == '.') {
				fprintf(stderr, "tried to access a variable as a struct when it is not\n");
				exit(1);
			}
			
			break;

		case SIZEOF:
			expect(SIZEOF);
			expect('[');
			if(token < B8 || token > BPTR) {
				fprintf(stderr, "sizeof needs a type\n");
				exit(1);
			}

			int type_size = int_pow(2, token - B8);
			if(token == BPTR) {
				void *test_size = NULL;
				type_size = sizeof(test_size);
			}

			eval_value = type_size;
			next();
			expect(']');
			break;

		default:
			eval_value = token_val;
			expect(NUM);
			break;
	}

	return eval_value;
}

value_t expr_tail(value_t left_val) {
	int index = -1;
	switch(token) {
		
		// XXX: Handle assigning for structs and struct props
		case '=':
			expect('=');
			if(struct_prop_def.value != NULL) {
				unsigned char *ptr = (unsigned char *) struct_prop_def.value;
				value_t expr_val = expr();
				for(int i = 0; i < struct_prop_def.type_size; i++) {
					ptr[i] = (unsigned char)((expr_val >> (i * 8)) & 0xff);
				}
				
				break;
			}
			
			if(found_ident) {
				found_ident = 0;
				memcpy(ident, ident_copy, IDENT_MAX);
				index = ident_var_index(1);
				value_t temp_expr = expr();
				assign_int_var(&(vars[index]), temp_expr);
				return var_to_int(vars[index]);
			}

			if(token < B8 || token > BPTR) {
				fprintf(stderr, "can't find type, when assigning to address\n");
				exit(1);
			}

			int type_size = token != BPTR
				? int_pow(2, token - B8)
				: (int) sizeof(void *);
			
			next();
			value_t expr_val = expr();
			unsigned char *ptr = (unsigned char *) left_val;
			for(int i = 0; i < type_size; i++) {
				ptr[i] = (unsigned char)((expr_val >> (i * 8)) & 0xff);
			}
			
			return expr_val;
		
		case '+': expect('+'); return expr_tail(left_val + value());
		case '-': expect('-'); return expr_tail(left_val - value());
		case '*': expect('*'); return expr_tail(left_val * value());
		case '/': 
			expect('/'); 
			value_t right_val = value();
			if(right_val == 0) {
				fprintf(stderr, "can't divide %ld by 0\n", left_val);
				exit(1);
			}
			
			return expr_tail(left_val / right_val);
			break;
		
		case '%': expect('%'); return expr_tail(left_val % value());
		case '<': expect('<'); return expr_tail(left_val < value());
		case '>': expect('>'); return expr_tail(left_val > value());
		case '&': expect('&'); return expr_tail(left_val & value());
		case '|': expect('|'); return expr_tail(left_val | value());
		case '^': expect('^'); return expr_tail(left_val ^ value());
		case EQ: expect(EQ); return expr_tail(left_val == value());
		case MORE_EQ: expect(MORE_EQ); return expr_tail(left_val >= value());
		case LESS_EQ: expect(LESS_EQ); return expr_tail(left_val <= value());
		case NOT_EQ: expect(NOT_EQ); return expr_tail(left_val != value());
		case SHL: expect(SHL); return expr_tail(left_val << value());
		case SHR: expect(SHR); return expr_tail(left_val >> value());
	}

	return left_val;
}

value_t expr() {
	value_t left_val = value();
	return expr_tail(left_val);
}

// statement parsing
void skip_block() {
	int brace_count = 0;
	do {
		brace_count += token == '{';
		brace_count -= token == '}';
		next();
	} while(brace_count > 0 && token != STOP);
	
	if(token == STOP) {
		fprintf(stderr, "unmatched braces\n");
		exit(1);
	}
}

enum {
	FLOW_IF,
	FLOW_LOOP
};

typedef struct {
	int type;
	char *start;
	int eval;
	int is_evaled;	
} flow_t;

flow_t *flow_stack = NULL;
int flow_stack_size = 0;
void stmt() {
	int inital = token;
	switch(inital) {
		case STOP:
			return;

		// if one of these opers are found here it has to be unary
		case '(':
		case ':': 
		case '@':
		case '-':
		case '~':
		case '!':
		case SIZEOF:
		case IDENT:

			// not redunant because fallthrough.
			if(token == IDENT) {
				memcpy(ident_copy, ident, IDENT_MAX);
				char *backtrack_src = src;
				next();
				if(token == '{') {
					expect('{');
					memcpy(ident, ident_copy, IDENT_MAX);
					struct_def_t *new_struct = make_struct_def();
					char *temp_src = src;
					src = backtrack_src;
					next();
					int ident_size = strlen(ident);
					new_struct->struct_name = safe_malloc(ident_size + 1);
					memcpy(new_struct->struct_name, ident, ident_size);
					new_struct->struct_name[ident_size] = 0;
					src = temp_src;
					while(token != '}' && token != STOP) {
						int type_size = token != BPTR
							? int_pow(2, token - B8)
							: (int) sizeof(value_t);

						next();
						if(token != '+' && token != '-') {
							fprintf(stderr, "you forgot to provide a +/- when declaring a struct\n");
							exit(1);
						}

						int is_pos = token == '+';
						next();
						add_struct_def(new_struct, (var_t) {
							ident,
							type_size,
							is_pos,
							NULL,
							NULL
						});

						next();
					}

					ARRAY_PUSH(struct_stack, struct_def_t *, new_struct);
					next();
					break;
				}

				token = IDENT;
				src = backtrack_src;
				next();
				int struct_index = index_struct_def(ident_copy);
				if(struct_index != -1) {
					struct_val_t *struct_val = make_struct_val(struct_stack[struct_index], ident);
					ident_var_add();
					vars[vars_size - 1].type_size = 0;
					vars[vars_size - 1].is_pos = 0;
					vars[vars_size - 1].value = NULL;
					vars[vars_size - 1].struct_val = struct_val;
					next();
					break;
				}
				
				token = IDENT;
				src = backtrack_src;
			}
			
		case NUM:
			fprintf(stderr, "expr %ld\n", expr());
			break;

		// skip b0 because b0 is only for functions	
		case B8:
		case B16:
		case B32:
		case B64:
			if(sizeof(void *) < 8) {
				fprintf(stderr, "you are not on a 64-bit computer\n");
				exit(1);
			}

		case BPTR:
			next();
			if(token != '+' && token != '-') {
				fprintf(stderr, "when declaring, a type needs to be followed by a +/-\n");
				exit(1);
			}

			int is_pos = token == '+';
			next();
			if(token != IDENT) {
				fprintf(stderr, "couldn't find name for variable name\n");
				exit(1);
			}
			
			ident_var_add();
			vars[vars_size - 1].type_size = int_pow(2, inital - B8);
			if(inital == BPTR) {
				vars[vars_size - 1].type_size = sizeof(void *);	
			}
			
			vars[vars_size - 1].is_pos = is_pos;
			vars[vars_size - 1].value = NULL;
			vars[vars_size - 1].struct_val = NULL;
			next();

			// XXX: functions use '[', not accounted for
			expect('=');
			value_t temp_expr = expr();
			assign_int_var(&(vars[vars_size - 1]), temp_expr);
			fprintf(stderr, "decl %ld\n", temp_expr);
			break;

		case IF:
			expect(IF);
			int result = expr();
			flow_t flow = {
				FLOW_IF,
				NULL,
				!!result,
				0
			};

			ARRAY_PUSH(flow_stack, flow_t, flow);
			if(!result) {
				skip_block();
				break;
			}

			break;

		case ELSE:
			expect(ELSE);
			if(flow_stack_size <= 0) {
				fprintf(stderr, "else without an if\n");
				exit(1);
			}
			
			if(flow_stack[flow_stack_size - 1].eval) {
				if(token == IF) {
					expect(IF);
					expr();
				}
				
				skip_block();
				break;
			}

			if(token == IF) {
				expect(IF);
				int result = expr();
				flow_t flow = {
					FLOW_IF,
					NULL,
					!!result,
					0
				};

				ARRAY_PUSH(flow_stack, flow_t, flow);
				if(!result) {
					break;
				}
			}

			break;

		case LOOP:
			flow_t current_flow = flow_stack_size <= 0

				// no flow, fill with zeros because current_flow will be ignored
				? (flow_t) { 0, 0, 0, 0 }
				: flow_stack[flow_stack_size - 1];
			
			if(flow_stack_size <= 0 || current_flow.type != LOOP || src != current_flow.start) {
				flow_t loop_flow = {
					FLOW_LOOP,
					src,
					0,
					0
				};

				ARRAY_PUSH(flow_stack, flow_t, loop_flow);
			}

			expect(LOOP);
			int loop_expr = expr();
			flow_stack[flow_stack_size - 1].eval = loop_expr;
			if(!loop_expr) {
				skip_block();
			}

			break;
		
		case '{':
			fprintf(stderr, "block enter %d -> %d\n", var_scopes_size, var_scopes_size + 1);
			expect('{');
			var_scope_add();
			break;

		case '}':
			fprintf(stderr, "block remove %d -> %d\n", var_scopes_size, var_scopes_size - 1);
			expect('}');
			var_scope_remove();
			if(flow_stack_size <= 0) {
				fprintf(stderr, "extra closing parens\n");
				exit(1);
			}

			flow_stack[flow_stack_size - 1].is_evaled = 1;
			if(flow_stack[flow_stack_size - 1].type == FLOW_LOOP) {
				src = flow_stack[flow_stack_size - 1].start;
			}

			break;

		default:
			fprintf(stderr, "unimplemented statement %d:%c index %d\n", inital, inital, (int)(src - start_src));
			exit(1);
	}
}

// main
void free_vars() {
	// not freeing stuff but this is the exit function so that's why it's here
	int line = 1;
	char *temp_src = start_src;
	for(; temp_src != src; temp_src++) {
		line += *temp_src == '\n';
	}
	
	fprintf(stderr, "this exit happened on line %d\n", line);
	NOT_NULL_FREE_SIZE(var_values);
	NOT_NULL_FREE_SIZE(var_scopes);
	NOT_NULL_FREE_SIZE(flow_stack);
	for(int i = 0; i < vars_size && vars != NULL; i++) {
		if(vars[i].name != NULL) {
			free(vars[i].name);
		}
		
		vars[i].name = NULL;
		free_struct_val(&(vars[i].struct_val));
	}

	NOT_NULL_FREE_SIZE(vars);
	free(start_src);
	src = NULL;
	start_src = NULL;
	if(struct_stack != NULL) {
		for(int i = 0; i < struct_stack_size; i++) {
			free_struct_def(&struct_stack[i]);
		}

		free(struct_stack);
	}

	struct_stack = NULL;
	struct_stack_size = 0;
}

int main() {
	fprintf(stderr, "Ctrl-C to exit, Ctrl-D to run.\n");
	var_scopes_size++;
	var_scopes = safe_realloc(var_scopes, sizeof(int) * var_scopes_size);
	var_scopes[0] = 0;
	atexit(free_vars);
	char input_char = 0;
	int src_size = 0;
	while((input_char = fgetc(stdin)) != -1) {
		src_size++;

		// hack for whitespace, next() needs whitespace to figure out num, not eof
		src = safe_realloc(src, sizeof(char) * (src_size + 2));
		src[src_size - 1] = input_char;
		src[src_size] = '\n';
		src[src_size + 1] = 0;
	}

	start_src = src;
	fprintf(stderr, "\ndebug log:\n");	
	next();
	while(token != STOP && *src && *src != -1) {
		stmt();
	}
}
