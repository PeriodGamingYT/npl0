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
	fprintf(stderr, "%d hit%s\n", __LINE__, #_x);

#define HARGS(...) \
	fprintf(stderr, "%d hit args ", __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n");

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

#define EXPECT(_x) \
	expect((_x), __LINE__)

#define PRINT_VALUE_BYTES(_x) \
	print_value_bytes(#_x, _x)

// lexing
// 32 (actually 31 because of null term) is plenty, if you need more than that, you're screwed anyway
#define IDENT_MAX 32
char ident[IDENT_MAX] = { 0 };
int ident_index = 0;
typedef intptr_t value_t;

// DEF: print_value_bytes

int token = 0;
value_t token_val = 0;
char *src = NULL;
char *start_src = NULL;
char *buffer_ptr = NULL;
void skip_comment() {
	if(*src == '\\') {
		src++;
		while(!IS_NEWLINE(*src) && !IS_STOP(*src)) {
			src++;
		}

		while(IS_NEWLINE(*src)) {
			src++;
		}

		if(IS_STOP(*src)) {
			token = STOP;
			return;
		}
	}
}

void next() {

	// whitespace
	if(IS_STOP(*src)) {
		token = STOP;
		return;
	}

	skip_comment();
	while(IS_WHITESPACE(*src)) {
		skip_comment();
		src++;
	}

	if(IS_STOP(*src)) {
		token = STOP;
		return;
	}

	skip_comment();

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
	skip_comment();
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
	skip_comment();
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
	skip_comment();
}

void expect(char x, int line) {
	if(token != x) {
		fprintf(stderr, "line %d expected %c:%d, but got a '%c':%d instead\n", line, x, x, token, token);
		exit(1);
	}

	next();
}

// memory alloc
void *safe_realloc(void *ptr, unsigned int size) {
	void *temp = realloc(ptr, size);
	if(temp == NULL) {
		if(ptr != NULL) {
			free(ptr);
		}
		
		fprintf(stderr, "failed realloc %p -> %p, %ud\n", ptr, temp, size);
		exit(1);
	}

	return temp;
}

void *safe_malloc(unsigned int size) {
	void *temp = malloc(size);
	if(temp == NULL) {
		fprintf(stderr, "failed malloc\n");
		exit(1);
	}

	return temp;
}

// structs
typedef struct struct_def_s {
	int size;
	int *is_pos;
	int *type_size;
	struct struct_def_s **struct_defs;
	char **name;
	char *struct_name;
} struct_def_t;

// DEF: print_struct_def

// part of vars
#define VALUE_MAX 8
typedef unsigned char var_value_t[VALUE_MAX];
typedef struct struct_val_s {
	struct_def_t *struct_def;
	var_value_t *vals;
	char *name;
	struct struct_val_s **struct_vals;
} struct_val_t;

// DEF: print_value_bytes

void free_struct_val(struct_val_t **struct_val) {
	NULL_RETURN_REF(struct_val);
	for(int i = 0; i < (*struct_val)->struct_def->size; i++) {
		struct_val_t *sub_struct_val = (*struct_val)->struct_vals[i];
		free_struct_val(&sub_struct_val);
	}

	NOT_NULL_FREE((*struct_val)->vals);
	NOT_NULL_FREE((*struct_val)->struct_vals);
	NOT_NULL_FREE((*struct_val)->name);
	NOT_NULL_FREE(*struct_val);
}

int struct_def_prop(struct_def_t *struct_def, char *name) {
	if(struct_def == NULL) {
		return -1;
	}

	for(int i = 0; i < struct_def->size; i++) {
		if(!strcmp(struct_def->name[i], name)) {
			return i;
		}
	}

	return -1;
}

int struct_val_prop(struct_val_t *struct_val, char *name) {
	if(struct_val == NULL) {
		return -1;
	}

	return struct_def_prop(struct_val->struct_def, name);
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
	struct_val->struct_vals = safe_malloc(sizeof(struct_val_t *) * struct_def->size);
	memset(struct_val->struct_vals, 0, sizeof(struct_val_t *) * struct_def->size);
	for(int i = 0; i < struct_def->size; i++) {
		if(struct_def->struct_defs[i] == NULL) {
			continue;
		}

		struct_val->struct_vals[i] = make_struct_val(struct_def->struct_defs[i], struct_def->name[i]);
	}
	
	return struct_val;
}

struct_def_t **struct_stack = NULL;
int struct_stack_size = 0;
int struct_def_index(char *name) {
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
		NULL,
		NULL
	};
	
	return result;
}

void free_struct_def(struct_def_t **struct_def) {
	NULL_RETURN_REF(struct_def);
	NOT_NULL_FREE((*struct_def)->is_pos);
	NOT_NULL_FREE((*struct_def)->type_size);
	NOT_NULL_FREE((*struct_def)->struct_defs);
	NOT_NULL_FREE((*struct_def)->struct_name);
	if((*struct_def)->name != NULL) {
		for(int i = 0; i < (*struct_def)->size; i++) {
			NOT_NULL_FREE((*struct_def)->name[i]);
		}

		free((*struct_def)->name);
		(*struct_def)->name = NULL;
	}

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

typedef struct {
	char *name;
	int type_size;
	int is_pos;
	struct_def_t *struct_def;
} struct_prop_t;

void add_struct_def(struct_def_t *struct_def, struct_prop_t prop) {
	struct_def->size++;
	struct_def->is_pos = safe_realloc(struct_def->is_pos, sizeof(int) * struct_def->size);
	struct_def->type_size = safe_realloc(struct_def->type_size, sizeof(int) * struct_def->size);
	struct_def->name = safe_realloc(struct_def->name, sizeof(char *) * struct_def->size);
	struct_def->struct_defs = safe_realloc(struct_def->struct_defs, sizeof(struct_def_t *) * struct_def->size);
	struct_def->is_pos[struct_def->size - 1] = prop.is_pos;
	struct_def->type_size[struct_def->size - 1] = prop.type_size;
	int name_size = strlen(prop.name);
	struct_def->name[struct_def->size - 1] = safe_malloc(name_size + 1);
	memcpy(struct_def->name[struct_def->size - 1], prop.name, name_size);
	struct_def->name[struct_def->size - 1][name_size] = 0;
	struct_def->struct_defs[struct_def->size - 1] = prop.struct_def;
}

// rest of vars
var_value_t *var_values = NULL;
int var_values_size = 0;
var_value_t *var_values_add() {
	ARRAY_PUSH_UNDEF(var_values, var_value_t);
	memset(var_values[var_values_size - 1], 0, sizeof(var_value_t));
	return &(var_values[var_values_size - 1]);
}

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
			EXPECT('(');
			eval_value = expr();
			EXPECT(')');
			break;

		case '!': EXPECT('!'); eval_value = !expr(); break;
		case '~': EXPECT('~'); eval_value = ~expr(); break;
		case '-': EXPECT('-'); eval_value = -expr(); break;
		case ':':
			EXPECT(':');
			int var_index = ident_var_index(1);
			eval_value = vars[var_index].struct_val == NULL
				? (value_t) vars[var_index].value
				: (value_t) vars[var_index].struct_val->vals;

			EXPECT(IDENT);
			break;

		case '#':
			EXPECT('#');
			EXPECT('(');
			value_t hash_base = expr();
			EXPECT(')');
			int hash_struct_index = struct_def_index(ident);
			if(hash_struct_index == -1) {
				fprintf(stderr, "couldn't find structure %s\n", ident);
				exit(1);
			}
			
			EXPECT(IDENT);
			EXPECT('>');
			struct_def_t *hash_struct_def = struct_stack[hash_struct_index];
			int hash_prop_index = struct_def_prop(hash_struct_def, ident);
			if(hash_prop_index == -1) {
				fprintf(stderr, "couldn't find structure property %s\n", ident);
				exit(1);
			}

			EXPECT(IDENT);
			found_ident = 0;
			struct_prop_def = (var_def_t) { NULL, 0, 0 };
			eval_value = (value_t) &(((value_t *) hash_base)[hash_prop_index]);
			break;

		case '@':
			EXPECT('@');
			if(token < B8 || token > BPTR) {
				fprintf(stderr, "when using '@', provide a type and then a +/-\n");
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

		case '.':
			if(current_struct_val == NULL) {
				fprintf(stderr, "missing dot to a struct\n");
				exit(1);
			}

			EXPECT('.');
			int prop_index = struct_val_prop(current_struct_val, ident);
			if(prop_index == -1) {
				fprintf(stderr, "couldn't find struct %s\n", ident);
				exit(1);
			}
			
			EXPECT(IDENT);
			var_t prop_var = {
				NULL,
				current_struct_val->struct_def->type_size[prop_index],
				current_struct_val->struct_def->is_pos[prop_index],
				&((current_struct_val->vals[prop_index])),
				NULL
			};

			struct_def_t *struct_def = current_struct_val->struct_def->struct_defs[prop_index];
			if(struct_def != NULL && token == '.') {
				current_struct_val = current_struct_val->struct_vals[prop_index];
				eval_value = value();
				break;
			}
			
			eval_value = var_to_int(prop_var);
			struct_prop_def = (var_def_t) {
				prop_var.value,
				prop_var.type_size,
				prop_var.is_pos
			};

			current_struct_val = NULL;
			found_ident = 0;
			break;
			
		case IDENT:
			if(current_struct_val != NULL) {
				memcpy(ident_copy, ident, IDENT_MAX);
				EXPECT(IDENT);
				int sub_prop_index = struct_val_prop(current_struct_val, ident);
				if(sub_prop_index == -1) {
					fprintf(stderr, "couldn't find structure property %s\n", ident_copy);
					exit(1);
				}
				
				current_struct_val = current_struct_val->struct_vals[sub_prop_index];
				eval_value = value();
				break;
			}
	
			int index = ident_var_index(1);
			var_t var = vars[index];
			eval_value = var_to_int(var);
			memcpy(ident_copy, ident, IDENT_MAX);
			EXPECT(IDENT);
			found_ident = 1;
			if(token == '.' && var.struct_val != NULL) {
				current_struct_val = var.struct_val;
				eval_value = value();
				current_struct_val = NULL;
			} else if(token == '.') {
				fprintf(stderr, "tried to access a variable as a struct when it is not\n");
				exit(1);
			}
			
			break;

		case SIZEOF:
			EXPECT(SIZEOF);
			EXPECT('[');
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
			EXPECT(']');
			break;

		default:
			if(token == NUM) {
				EXPECT(NUM);
				eval_value = token_val;
			}

			break;
	}

	return eval_value;
}

value_t expr_tail(value_t left_val) {
	switch(token) {		
		case '=':
			EXPECT('=');
			if(struct_prop_def.value != NULL) {
				unsigned char *ptr = (unsigned char *) struct_prop_def.value;
				value_t expr_val = expr();
				for(int i = 0; i < struct_prop_def.type_size; i++) {
					ptr[i] = (unsigned char)((expr_val >> (i * 8)) & 0xff);
				}

				found_ident = 0;
				current_struct_val = NULL;
				return *((value_t *) struct_prop_def.value);
			}
			
			if(found_ident) {
				int index = -1;
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
		
		case '+': EXPECT('+'); return expr_tail(left_val + value());
		case '-': EXPECT('-'); return expr_tail(left_val - value());
		case '*': EXPECT('*'); return expr_tail(left_val * value());
		case '/': 
			EXPECT('/'); 
			value_t right_val = value();
			if(right_val == 0) {
				fprintf(stderr, "can't divide %ld by 0\n", left_val);
				exit(1);
			}
			
			return expr_tail(left_val / right_val);
			break;
		
		case '%': EXPECT('%'); return expr_tail(left_val % value());
		case '<': EXPECT('<'); return expr_tail(left_val < value());
		case '>': EXPECT('>'); return expr_tail(left_val > value());
		case '&': EXPECT('&'); return expr_tail(left_val & value());
		case '|': EXPECT('|'); return expr_tail(left_val | value());
		case '^': EXPECT('^'); return expr_tail(left_val ^ value());
		case EQ: EXPECT(EQ); return expr_tail(left_val == value());
		case MORE_EQ: EXPECT(MORE_EQ); return expr_tail(left_val >= value());
		case LESS_EQ: EXPECT(LESS_EQ); return expr_tail(left_val <= value());
		case NOT_EQ: EXPECT(NOT_EQ); return expr_tail(left_val != value());
		case SHL: EXPECT(SHL); return expr_tail(left_val << value());
		case SHR: EXPECT(SHR); return expr_tail(left_val >> value());
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
		case '#':
		case SIZEOF:
		case IDENT:

			// not redunant because fallthrough.
			if(token == IDENT) {
				memcpy(ident_copy, ident, IDENT_MAX);
				char *backtrack_src = src;
				next();
				if(token == '{') {
					EXPECT('{');
					char first_prop_name[IDENT_MAX] = { 0 };
					memcpy(first_prop_name, ident, IDENT_MAX);
					memcpy(ident, ident_copy, IDENT_MAX);
					struct_def_t *new_struct = make_struct_def();
					int ident_size = strlen(ident);
					new_struct->struct_name = safe_malloc(ident_size + 1);
					memcpy(new_struct->struct_name, ident, ident_size);
					new_struct->struct_name[ident_size] = 0;
					memcpy(ident, first_prop_name, IDENT_MAX);
					while(token != '}' && token != STOP) {
						if(token == IDENT) {
							int struct_index = struct_def_index(ident);
							if(struct_index == -1) {
								free_struct_def(&new_struct);
								fprintf(stderr, "couldn't find struct %s\n", ident);
								exit(1);
							}
							
							next();
							add_struct_def(new_struct, (struct_prop_t) {
								ident,
								0,
								0,
								struct_stack[struct_index]
							});

							next();
							continue;
						}
						
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
						add_struct_def(new_struct, (struct_prop_t) {
							ident,
							type_size,
							is_pos,
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
				int struct_index = struct_def_index(ident_copy);
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
			EXPECT('=');
			value_t temp_expr = expr();
			assign_int_var(&(vars[vars_size - 1]), temp_expr);
			fprintf(stderr, "decl %ld\n", temp_expr);
			break;

		case IF:
			EXPECT(IF);
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
			EXPECT(ELSE);
			if(flow_stack_size <= 0) {
				fprintf(stderr, "else without an if\n");
				exit(1);
			}
			
			if(flow_stack[flow_stack_size - 1].eval) {
				if(token == IF) {
					EXPECT(IF);
					expr();
				}
				
				skip_block();
				break;
			}

			if(token == IF) {
				EXPECT(IF);
				int if_result = expr();
				flow_t if_flow = {
					FLOW_IF,
					NULL,
					!!if_result,
					0
				};

				ARRAY_PUSH(flow_stack, flow_t, if_flow);
				if(!if_result) {
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

			EXPECT(LOOP);
			int loop_expr = expr();
			flow_stack[flow_stack_size - 1].eval = loop_expr;
			if(!loop_expr) {
				skip_block();
			}

			break;
		
		case '{':
			fprintf(stderr, "block enter %d -> %d\n", var_scopes_size, var_scopes_size + 1);
			EXPECT('{');
			var_scope_add();
			break;

		case '}':
			fprintf(stderr, "block remove %d -> %d\n", var_scopes_size, var_scopes_size - 1);
			EXPECT('}');
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
	if(src == NULL || IS_STOP(*src)) {
		fprintf(stderr, "this is an empty program\n");
		exit(0);
	}
	
	next();
	while(token != STOP && *src && *src != -1) {
		stmt();
	}
}
