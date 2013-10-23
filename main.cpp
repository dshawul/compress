#include "codec.h"
#include <cstring>

void print_help() {
	printf("Usage: compress [options]<input> -o <output>\n\t"
		"-c -- compress\n\t"
		"-d -- decompress\n\t"
		"-o -- output directory\n\t"
		"-crc -- check integrity of file\n\t"
		"-h -- show this help message\n\t"
		);
}

int main(int argc, char *argv[]) {
	enum {
		NONE = 0,COMPRESS,DECOMPRESS,CRC,CRCW
	};
	COMP_INFO comp_info;
	int comp_mode = COMPRESS;
	FILE *input = stdin,*output = 0;
	char opath[256] = "";

	for(int i = 1; i < argc; i++) {
		if(!strcmp(argv[i],"-c")) {
			comp_mode = COMPRESS;
		} else if(!strcmp(argv[i],"-d")) {
			comp_mode = DECOMPRESS;
		} else if(!strcmp(argv[i],"-crc")) {
			comp_mode = CRC;
		} else if(!strcmp(argv[i],"-crcw")) {
			comp_mode = CRCW;
		} else if(!strcmp(argv[i],"-h")) {
			print_help();
		} else if(!strcmp(argv[i],"-o")) {
			strcpy(opath,argv[++i]);
		} else {
			char name[256],oname[256];
			strcpy(name,argv[i]);
			strcpy(oname,opath);
			strcat(oname,name);

			input = fopen(name,"rb");
			if(comp_mode == CRC) {
				strcpy(comp_info.output_fname,name);
				comp_info.write_crc(false);
			} else if(comp_mode == CRCW) {
				strcpy(comp_info.output_fname,name);
				comp_info.write_crc(true);
			} else if(comp_mode == COMPRESS) {
				strcat(oname,".cmp");
				output = fopen(oname,"wb");
				strcpy(comp_info.output_fname,oname);
				comp_info.compress(input,output);
				fclose(output);
				comp_info.write_crc(true);
			} else if(comp_mode == DECOMPRESS) {
				oname[strlen(oname) - 4] = 0;
				output = fopen(oname,"wb");
				comp_info.decompress(input,output);
			}
		}
	}

	return 0;
}
