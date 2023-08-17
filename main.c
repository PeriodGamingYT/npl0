#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// lexing
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
	IF,
	LOOP,
	LINK,
	STACK,
	HEAP,
	NOFREE,
	RODATA,
	SIZEOF
};

int token = 0;
int token_val = 0;
char *src = NULL;
char *buffer_ptr = NULL;

// 32 (actually 31 because of null term) is plenty, if you need more than that, you're screwed anyway
#define IDENT_MAX 32
char ident[IDENT_MAX] = { 0 };
int ident_index = 0;
#define IS_IDENT_START(_x) \
	((_x >= 'A' && _x <= 'Z') || (_x >= 'a' && _x <= 'z') || (_x == '_'))

#define IS_NUMBER(_x) \
	(_x >= '0' && _x <= '9')

#define IS_STOP(_x) \
	(!(_x) || _x == -1)

#define ARRAY_SIZE(_x) \
	((int)(sizeof(_x) / sizeof((_x)[0])))

void next() {

	// eof or 0
	if(IS_STOP(*src)) {
		token = STOP;
		return;
	}

	// comments
	if(*src == '\\') {
		while(*src != '\n' && !IS_STOP(*src)) {
			src++;
		}
	}

	// whitespace
	while(*src == ' ' || *src == '\t' || *src == '\n') {
		src++;
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
			IF, 
			LOOP, 
			LINK, 
			STACK, 
			HEAP, 
			NOFREE,
			SIZEOF
		};

		const char *reserved[] = {
			"b0",
			"b8",
			"b16",
			"b32",
			"if",
			"loop",
			"link",
			"stack",
			"heap",
			"nofree",
			"sizeof"
		};

		for(int i = 0; i < ARRAY_SIZE(reserved); i++) {
			if(strcmp(reserved[i], ident) == 0) {
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

// variables
void *safe_realloc(void *ptr, int size) {
	void *temp = realloc(ptr, size);
	if(temp == NULL) {
		if(ptr != NULL) {
			free(ptr);
		}
		
		fprintf(stderr, "failed realloc %p -> %p, %d\n", ptr, temp, size);
		exit(1);
	}

	return temp;
}

void *safe_malloc(int size) {
	void *temp = malloc(size);
	if(temp == NULL) {
		fprintf(stderr, "failed malloc\n");
		exit(1);
	}

	return temp;
}

#define VALUE_MAX 4

// value_t is here for possible encapsulation of vars in the future
typedef int value_t;
typedef unsigned char var_value_t[VALUE_MAX];
typedef struct var_s {
	char *name;
	int type_size;
	int is_pos;
	int ptr_count;
	var_value_t *value;

	// using w/ ptr ref in expr_tail()
	struct var_s *real;
} var_t;

var_value_t *var_values = NULL;
int var_values_size = 0;
var_value_t *var_values_add() {
	var_values_size++;
	var_values = safe_realloc(var_values, sizeof(var_value_t) * var_values_size);
	memset(var_values[var_values_size - 1], 0, sizeof(var_value_t));
	return &(var_values[var_values_size - 1]);
}

// (*(var->value))[i] parens are needed!
void assign_int_var(var_t *var, int x) {
	if(var->value == NULL) {
		var->value = var_values_add();
	}
	
	memset(*(var->value), 0, sizeof(var_value_t));
	for(int i = 0; i < var->type_size && i < (int) sizeof(x); i++) {
		(*(var->value))[i] = (unsigned char)((x >> (i * 8)) & 0xff);
	}
}

int var_to_int(var_t var) {
	unsigned int result = 0;
	for(int i = 0; i < var.type_size; i++) {
		result |= (*(var.value))[i] << (i * 8);
	}

	unsigned int min_two_comp = 1 << ((var.type_size * 8) - 1);
	if(!var.is_pos && result >= min_two_comp) {
		return result - (min_two_comp * 2);
	}
	
	return result;
}

var_t *vars = NULL;
int vars_size = 0;
int *var_scopes = NULL;
int var_scopes_size = 0;
void ident_var_add() {
	vars_size++;
	vars = safe_realloc(vars, sizeof(var_t) * vars_size);
	int ident_size = strlen(ident);
	vars[vars_size - 1].name = safe_malloc(sizeof(char) * (ident_size + 1));
	memcpy(vars[vars_size - 1].name, ident, sizeof(char) * ident_size);
	vars[vars_size - 1].name[ident_size] = 0;
	var_scopes[var_scopes_size - 1]++;
}

int ident_var_index(int is_last_error) {
	for(int i = 0; i < vars_size; i++) {
		if(strcmp(vars[i].name, ident) == 0) {
			return i;
		}
	}

	if(is_last_error) {
		fprintf(stderr, "nonexistant variable\n");
		exit(1);
	}
	
	ident_var_add();
	return vars_size - 1;
}

void var_scope_add() {
	var_scopes_size++;
	var_scopes = safe_realloc(var_scopes, sizeof(int) * var_scopes_size);
	var_scopes[var_scopes_size - 1] = 0;
}

void var_scope_remove() {
	if(var_scopes_size <= 0) {
		fprintf(stderr, "can't remove scope any further\n");
		exit(1);
	}
	
	int size = var_scopes_size - 1;
	int scope_size = var_scopes[var_scopes_size - 1];
	for(int i = size; i >= size - scope_size; i--) {
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

// expr parsing
char ident_copy[IDENT_MAX] = { 0 };
value_t expr();
var_t *expr_values = NULL;
int expr_values_size = 0;
value_t value() {
	int value = 0;
	if(token == STOP || !(*src) || *src == -1) {
		fprintf(stderr, "can't find a token to get value of, found end of file instead\n");
		exit(1);
	}

	switch(token) {
		case '(':
			expect('(');
			value = expr();
			expect(')');
			break;

		case '!': expect('!'); value = !expr(); break;
		case '~': expect('~'); value = ~expr(); break;
		case '-': expect('-'); value = -expr(); break;

		// :a will deref a, ref is in expr_tail()
		case ':':
			expect(':');
			if(expr_values == NULL || expr_values[expr_values_size - 1].value != NULL) {
				expr_values_size++;
				expr_values = safe_realloc(expr_values, sizeof(var_t) * expr_values_size);
				expr_values[expr_values_size - 1] = (var_t) {
					NULL,
					-1,
					-1,
					0,
					NULL,
					NULL
				};
			}

			expr_values[expr_values_size - 1].ptr_count++;
			break;
		
		case IDENT:

			// deref first
			var_t var = vars[ident_var_index(1)];
			var.real = &(vars[ident_var_index(1)]);
			if(expr_values != NULL && expr_values[expr_values_size - 1].value == NULL) {
				int deref_count = var.ptr_count - expr_values[expr_values_size - 1].ptr_count;
				if(deref_count < 0) {
					fprintf(stderr, "pointer over dereferenced (%d over)\n", -deref_count);
					exit(1);
				}

				while(deref_count-- >= 0) {
					if(var.value == NULL) {
						fprintf(stderr, "dereferenced uninitalized pointer\n");
						exit(1);
					}

					// NOTE: cast to ptr from int, don't try to fix but keep tabs on it
					var = *((var_t *) var_to_int(var));
				}

				expr_values_size--;
				expr_values = safe_realloc(expr_values, sizeof(var_t) * expr_values_size);
			}

			expr_values_size++;
			expr_values = safe_realloc(expr_values, sizeof(var_t) * expr_values_size);
			expr_values[expr_values_size - 1] = var;
			value = var_to_int(var);
			memcpy(ident_copy, ident, sizeof(char) * IDENT_MAX);
			expect(IDENT);
			break;

		default:
			value = token_val;
			expect(NUM);
			break;
	}

	return value;
}

value_t expr_tail(int left_val) {
	switch(token) {
		case ':':

			// NOTE: cast from ptr to int, don't try to fix but keep tabs on it.
			expect(':');
			return expr_tail((value_t) (expr_values[expr_values_size - 1].real));
		
		case '=':
			expect('=');
			memcpy(ident, ident_copy, sizeof(char) * IDENT_MAX);
			int index = ident_var_index(1);
			assign_int_var(
				expr_values == NULL 
					? &(vars[index])
					: expr_values[expr_values_size - 1].real,
				
				expr()
			);
			
			return var_to_int(vars[index]);
		
		case '+': expect('+'); return expr_tail(left_val + value());
		case '-': expect('-'); return expr_tail(left_val - value());
		case '*': expect('*'); return expr_tail(left_val * value());
		case '/': 
			expect('/'); 
			int right_val = value();
			if(right_val == 0) {
				fprintf(stderr, "can't divide %d by 0\n", left_val);
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
	int left_val = value();
	return expr_tail(left_val);
}

// statement parsing
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

void stmt() {
	int inital = token;
	switch(inital) {

		// if one of these opers are found here it has to be unary
		case ':': 
		case '-':
		case '~':
		case '!':
		case IDENT:
		
			// XXX: saying ident followed by a block is a struct, that isn't handled
		case NUM:
			printf("%d\n", expr());
			break;

		// skip b0 because b0 is only for functions	
		case B8:
		case B16:
		case B32:
			next();
			int ptr_count = 0;
			if(token == ':') {
				expect(':');
				ptr_count = 1;
				while(token == ':') {
					ptr_count++;
					expect(':');
				}
			}
			
			if(token != '+' && token != '-') {
				fprintf(stderr, "type needs to be followed by a +/-\n");
				exit(1);
			}

			// NOTE: doesn't check for '-' because there can only be a + or a -, change this if fixed/floating point is added
			int is_pos = token == '+';
			next();
			if(token != IDENT) {
				fprintf(stderr, "couldn't find name for variable name\n");
				exit(1);
			}
			
			ident_var_add();
			vars[vars_size - 1].type_size = int_pow(2, inital - B8);
			vars[vars_size - 1].is_pos = is_pos;
			vars[vars_size - 1].ptr_count = ptr_count;
			vars[vars_size - 1].real = NULL;
			vars[vars_size - 1].value = NULL;
			next();

			// XXX: functions use '[', not accounted for
			expect('=');
			assign_int_var(&(vars[vars_size - 1]), expr());
			break;
	}

	if(expr_values != NULL) {
		free(expr_values);
	}
}

// main
void free_vars() {
	if(expr_values != NULL) {
		free(expr_values);
	}
	
	expr_values = NULL;
	expr_values_size = 0;
	free(var_values);
	var_values = NULL;
	var_values_size = 0;
	free(var_scopes);
	var_scopes = NULL;
	var_scopes_size = 0;
	for(int i = 0; i < vars_size; i++) {
		free(vars[i].name);
	}

	free(vars);
	vars = NULL;
	vars_size = 0;
}

#define BUFFER_MAX 128
int main() {
	char buffer[BUFFER_MAX] = { 0 };
	buffer_ptr = &buffer[0];
	size_t buffer_size = BUFFER_MAX;
	printf("Ctrl-C to exit\n");
	var_scopes_size++;
	var_scopes = safe_realloc(var_scopes, sizeof(int) * var_scopes_size);
	var_scopes[0] = 0;
	for(;;) {
		printf("> ");
		getline(&buffer_ptr, &buffer_size, stdin);
		src = buffer_ptr;
		next();
		while(token != STOP && *src && *src != -1) {
			stmt();
		}

		memset(buffer, 0, sizeof(char) * BUFFER_MAX);
	}

	free_vars();
	return 0;
}
