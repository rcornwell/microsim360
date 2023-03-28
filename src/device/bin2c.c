#include <stdio.h>

int main(int argc, char** argv) {
    char* name = argv[1];
	char* fn_in = argv[2];
	char* fn_out = argv[3];
	FILE* f_in = fopen(fn_in, "r");
	FILE* f_out = fopen(fn_out, "w");
    fprintf(f_out, "#include <stdint.h>\nstatic const uint8_t %s[] = {\n", name);
	unsigned long n = 0;
	while(!feof(f_in)) {
		unsigned char c;
		if(fread(&c, 1, 1, f_in) == 0) break;
		fprintf(f_out, "0x%.2X,", (int)c);
		++n;
		if(n % 10 == 0) fprintf(f_out, "\n");
	}
	fclose(f_in);
	fprintf(f_out, "};\n");
    fclose(f_out);
}
