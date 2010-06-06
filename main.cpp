#include "codec.h"

enum {
	NONE,COMPRESS = 1,DECOMPRESS = 2,INPUT_FILE = 4,OUTPUT_FILE = 8
};

void main(int argc, char *argv[]) {

	COMP_INFO comp_info;
	int comp_mode = NONE;
	char input_fname[256],output_fname[256];
	
	for(int i = 0; i < argc; i++) {
		if(!strcmp(argv[i],"-c")) {
			comp_mode |= COMPRESS;
		} else if(!strcmp(argv[i],"-d")) {
			comp_mode |= DECOMPRESS;
		} else if(!strcmp(argv[i],"-i")) {
			strcpy(input_fname,argv[i+1]);
			comp_mode |= INPUT_FILE;
		} else if(!strcmp(argv[i],"-o")) {
			strcpy(output_fname,argv[i+1]);
			comp_mode |= OUTPUT_FILE;
		} 
	}

	if(comp_mode == (COMPRESS | INPUT_FILE | OUTPUT_FILE)) {
		comp_info.compress(input_fname,output_fname);
	} else if(comp_mode == (DECOMPRESS | INPUT_FILE | OUTPUT_FILE) ){
		comp_info.decompress(input_fname,output_fname);
	}
}
