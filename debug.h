// use this to debug things
void print_struct_val(struct_val_t *struct_val) {
	if(struct_val == NULL) {
		fprintf(stderr, "struct_val is null\n\n");
		return;
	}
	
	fprintf(stderr, "struct_val\n");
	print_struct_def(struct_val->struct_def);
	if(struct_val->name != NULL) {
		fprintf(stderr, "name %s\n\n", struct_val->name);
	}
	
	if(struct_val->struct_def == NULL) {
		fprintf(stderr, "struct_def in struct_val is null\n\n");
	}
	
	for(int i = 0; struct_val->struct_def != NULL && i < struct_val->struct_def->size; i++) {
		fprintf(stderr, "vals %d: %ld\n", i, (value_t)(struct_val->vals[i]));
	}
}

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

#define PRINT_VALUE_BYTES(_x) \
	print_value_bytes(#_x, _x)

void print_value_bytes(const char *message, value_t value) {
	fprintf(stderr, "value %s\n", message);
	unsigned char *value_ptr = (unsigned char *) &value;
	for(int i = 0; i < (int) sizeof(value_t); i++) {
		fprintf(stderr, "\t%d: %d\n", i, (unsigned char) value_ptr[i]);
	}

	fprintf(stderr, "\n");
}
