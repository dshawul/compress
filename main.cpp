#include "codec.h"
#include <cstring>

void print_help() {
	printf("Usage: compress [options] -i <input> -o <output>\n\t"
		"-c -- compress\n\t"
		"-d -- decompress\n\t"
		"-i -- input file\n\t"
		"-o -- output file\n\t"
		"-h -- show this help message\n"
		);
}
int main(int argc, char *argv[]) {
	enum {
		NONE = 0,COMPRESS = 1,DECOMPRESS = 2
	};
	COMP_INFO comp_info;
	int comp_mode = COMPRESS;
	FILE *input = stdin,*output = stdout;
	
	for(int i = 0; i < argc; i++) {
		if(!strcmp(argv[i],"-c")) {
			comp_mode = COMPRESS;
		} else if(!strcmp(argv[i],"-d")) {
			comp_mode = DECOMPRESS;
		} else if(!strcmp(argv[i],"-i")) {
			input = fopen(argv[i+1],"rb");
		} else if(!strcmp(argv[i],"-o")) {
			output = fopen(argv[i+1],"wb");
		} else if(!strcmp(argv[i],"-h")) {
			print_help();
		}
	}

	if(comp_mode == COMPRESS) {
		comp_info.compress(input,output);
	} else if(comp_mode == DECOMPRESS){
		comp_info.decompress(input,output);
	} else {
		print_help();
	}

	return 0;
}
