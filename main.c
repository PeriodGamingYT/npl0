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
	B64,
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
			B64, 
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
			"b64",
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
		free(ptr);
		fprintf(stderr, "failed realloc\n");
		exit(1);

		// unreachable
		return NULL;
	}

	return temp;
}

void *safe_malloc(int size) {
	void *temp = malloc(size);
	if(temp == NULL) {
		fprintf(stderr, "failed malloc\n");
		exit(1);

		// unreachable
		return NULL;
	}

	return temp;
}

typedef struct {
	char *name;
	int type_size;
	int is_pos;

	// XXX: types need to be respected, use char[8] later on
	int value;
} var_t;

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

		// unreachable
		return -1;
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

void free_vars() {
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

// expr parsing
char ident_copy[IDENT_MAX] = { 0 };
int expr();
int value() {
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
		case IDENT:
			value = vars[ident_var_index(1)].value;
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

int expr_tail(int left_val) {
	switch(token) {
		case '=':
			expect('=');
			memcpy(ident, ident_copy, sizeof(char) * IDENT_MAX);
			int index = ident_var_index(1);
			vars[index].value = expr_tail(value());
			return vars[index].value;
		
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

int expr() {
	int left_val = value();
	return expr_tail(left_val);
}

// statement parsing
int int_pow(int base, int exp) {
	int orig_base = base;
	while(exp--) {
		base *= orig_base;
	}

	return base;
}

void stmt() {
	int inital = token;
	switch(inital) {
		case IDENT:
		
			// XXX: saying ident followed by a block is a struct, that isn't handled
		case NUM:
			printf("%d\n", expr());
			break;

		// skip b0 because b0 is only for functions
		// XXX: use ":" for ptr stuff
		case B8:
		case B16:
		case B32:
		case B64:
			next();
			if(token != '+' && token != '-') {
				fprintf(stderr, "type needs to be followed by a +/-\n");
				exit(1);

				// unreachable
				return;
			}

			// XXX?: doesn't check for '-' because there can only be a + or a -, change this if fixed/floating point is added
			int is_pos = token == '+';
			next();
			if(token != IDENT) {
				fprintf(stderr, "couldn't find name for variable name\n");
				exit(1);

				// unreachable
				return;
			}
			
			ident_var_add();
			vars[vars_size - 1].type_size = int_pow(2, inital - B8);
			vars[vars_size - 1].is_pos = is_pos;
			next();

			// XXX: functions use '[', not accounted for
			// XXX: Types are not respected, change how expr returns stuff to fix
			expect('=');
			vars[vars_size - 1].value = expr();
			break;
	}
}

// main
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
