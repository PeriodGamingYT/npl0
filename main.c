#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define H(_x) \
	fprintf(stderr, "hit%s\n", #_x);

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

typedef uintptr_t value_t;
int token = 0;
value_t token_val = 0;
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

#define VALUE_MAX 8
typedef unsigned char var_value_t[VALUE_MAX];
typedef struct var_s {
	char *name;
	int type_size;
	int is_pos;
	var_value_t *value;
} var_t;

var_value_t *var_values = NULL;
int var_values_size = 0;
var_value_t *var_values_add() {
	var_values_size++;
	var_values = safe_realloc(var_values, sizeof(var_value_t) * var_values_size);
	memset(var_values[var_values_size - 1], 0, sizeof(var_value_t));
	return &(var_values[var_values_size - 1]);
}

value_t var_to_int(var_t var) {
	value_t result = *((value_t *) var.value);
	value_t min_two_comp = 1 << ((var.type_size * 8) - 1);
	if(!var.is_pos && result >= min_two_comp) {
		return result - (min_two_comp * 2);
	}

	return result;
}

// (*(var->value))[i] parens are needed!
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
value_t value() {
	value_t value = 0;
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
		case ':':
			expect(':');
			if(token != IDENT) {
				fprintf(stderr, "a : needs to be after or before an identifier");
				exit(1);
			}

			int index = ident_var_index(1);
			value_t var_val = var_to_int(vars[index]);
			expect(IDENT);
			found_ident = 0;
			return var_val;
		
		case IDENT:
			var_t var = vars[ident_var_index(1)];
			value = var_to_int(var);
			memcpy(ident_copy, ident, sizeof(char) * IDENT_MAX);
			expect(IDENT);
			found_ident = 1;
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

			value = type_size;
			next();
			expect(']');
			break;

		default:
			value = token_val;
			expect(NUM);
			break;
	}

	return value;
}

value_t expr_tail(value_t left_val) {
	int index = -1;
	switch(token) {
		case ':':
			expect(':');
			if(found_ident) {
				found_ident = 0;
				memcpy(ident, ident_copy, sizeof(char) * IDENT_MAX);
				index = ident_var_index(1);
				if(token < B8 || token > B64) {
					return expr_tail((value_t)(*(vars[index].value)));
				}
			}

			if(token < B8 || token > B64) {
				fprintf(stderr, "if you are going to use a colon with a number, give a type and a +/- with it\n");
				exit(1);
			}

			int val_size = int_pow(2, token - B8);
			next();
			if(token != '+' && token != '-') {
				fprintf(stderr, "if a colon is followed by a type, give a +/-\n");
				exit(1);
			}
			
			int val_pos = token == '+';
			value_t val = found_ident
				? *((value_t *) *((value_t *) vars[index].value))

				// can't do cast or else it will get 8 bytes no matter what
				: 0;

			unsigned char *left_ptr = (unsigned char *) left_val;

			// edge case handling (for above) happens here
			if(!found_ident) {
				for(int i = 0; i < val_size; i++) {
					val |= (value_t)(left_ptr[i]) << (i * 8);
				}
			}
			
			value_t min_two_comp = 1 << ((val_size * 8) - 1);
			if(!val_pos && val >= min_two_comp) {
				return val - (min_two_comp * 2);
			}

			next();
			return expr_tail(val);
		
		case '=':
			expect('=');
			if(found_ident) {
				found_ident = 0;
				memcpy(ident, ident_copy, sizeof(char) * IDENT_MAX);
				index = ident_var_index(1);
				value_t temp_expr = expr();
				assign_int_var(&(vars[index]), temp_expr);
				return var_to_int(vars[index]);
			}

			if(token < B8 || token > BPTR) {
				fprintf(stderr, "can't find type, when assigning to address\n");
				exit(1);
			}

			int type_size = int_pow(2, token - B8);
			if(token == BPTR) {
				void *test_size = NULL;
				type_size = sizeof(test_size);
			}
			
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

int last_if_result = 0;
void stmt() {
	int inital = token;
	switch(inital) {

		// if one of these opers are found here it has to be unary
		case '(':
		case ':': 
		case '-':
		case '~':
		case '!':
		case SIZEOF:
		case IDENT:
		
			// XXX: saying ident followed by a block is a struct, that isn't handled
		case NUM:
			printf("expr %ld\n", expr());
			break;

		// skip b0 because b0 is only for functions	
		case B8:
		case B16:
		case B32:
		case B64:
			void *test_size = NULL;
			if(sizeof(test_size) < 8) {
				fprintf(stderr, "you are not on a 64-bit computer\n");
				exit(1);
			}

		case BPTR:
			next();			
			if(token != '+' && token != '-') {
				fprintf(stderr, "type needs to be followed by a +/-\n");
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
				vars[vars_size - 1].type_size = sizeof(test_size);	
			}
			
			vars[vars_size - 1].is_pos = is_pos;
			vars[vars_size - 1].value = NULL;
			next();

			// XXX: functions use '[', not accounted for
			expect('=');
			value_t temp_expr = expr();
			assign_int_var(&(vars[vars_size - 1]), temp_expr);
			printf("decl %ld\n", temp_expr);
			break;

		case IF:
			expect(IF);
			int result = expr();
			last_if_result = !!result;
			if(!result) {
				skip_block();
				break;
			}

			goto else_skip;

		case ELSE:
			expect(ELSE);
			if(last_if_result) {
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
				last_if_result = !!result;
				if(!result) {
					break;
				}
			}

else_skip:
		case '{':
			printf("block enter %d -> %d\n", var_scopes_size, var_scopes_size + 1);
			expect('{');
			var_scope_add();
			break;

		case '}':
			printf("block remove %d -> %d\n", var_scopes_size, var_scopes_size - 1);
			expect('}');
			var_scope_remove();

			// hack to skip an if chain
			last_if_result = 1;
			break;
	}
}

// main
void free_vars() {
	if(var_values != NULL) {
		free(var_values);
	}
	
	var_values = NULL;
	var_values_size = 0;
	if(var_scopes != NULL) {
		free(var_scopes);
	}
	
	var_scopes = NULL;
	var_scopes_size = 0;
	for(int i = 0; i < vars_size && vars != NULL; i++) {
		if(vars[i].name == NULL) {
			continue;
		}
		
		free(vars[i].name);
		vars[i].name = NULL;
	}

	if(vars != NULL) {
		free(vars);
	}

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
	atexit(free_vars);
	for(;;) {
		printf("%s", var_scopes_size == 1 ? "> " : "  ");
		getline(&buffer_ptr, &buffer_size, stdin);
		src = buffer_ptr;
		next();
		while(token != STOP && *src && *src != -1) {
			stmt();
		}

		memset(buffer, 0, sizeof(char) * BUFFER_MAX);
	}
}
