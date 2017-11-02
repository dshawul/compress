#include "codec.h"
#include <cstring>

void print_help() {
    printf("Usage: compress [options]<input> -o <output>\n\t"
        "-c -- compress\n\t"
        "-d -- decompress\n\t"
        "-clz -- compress using lempel ziv\n\t"
        "-dlz -- decompress using lempel ziv\n\t"
        "-chf -- compress lz compressed data using huffman\n\t"
        "-dhf -- decompress huffman compressed data\n\t"
        "-o -- output directory\n\t"
        "-crc -- check integrity of file\n\t"
        "-h -- show this help message\n\t"
        );
}

int main(int argc, char *argv[]) {
    enum {
        NONE = 0,COMPRESS,COMPRESS_HUF,COMPRESS_LZ,DECOMPRESS,DECOMPRESS_HUF,DECOMPRESS_LZ,CRC,CRCW
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
        } else if(!strcmp(argv[i],"-clz")) {
            comp_mode = COMPRESS_LZ;
        } else if(!strcmp(argv[i],"-dlz")) {
            comp_mode = DECOMPRESS_LZ;
        } else if(!strcmp(argv[i],"-chf")) {
            comp_mode = COMPRESS_HUF;
        } else if(!strcmp(argv[i],"-dhf")) {
            comp_mode = DECOMPRESS_HUF;
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
            } else if(comp_mode == COMPRESS || comp_mode == COMPRESS_LZ || comp_mode == COMPRESS_HUF) {
                if(comp_mode == COMPRESS) 
                    strcat(oname,".cmp");
                else if (comp_mode == COMPRESS_HUF) {
                    oname[strlen(oname) - 3] = 0;
                    strcat(oname,".cmp");
                } else  
                    strcat(oname,".lz");
                output = fopen(oname,"wb");
                strcpy(comp_info.output_fname,oname);
                comp_info.compress(input,output,comp_mode - COMPRESS);
                fclose(output);
                comp_info.write_crc(true);
            } else if(comp_mode == DECOMPRESS || comp_mode == DECOMPRESS_LZ || comp_mode == DECOMPRESS_HUF) {
                if(comp_mode == DECOMPRESS) 
                    oname[strlen(oname) - 4] = 0;
                else if(comp_mode == DECOMPRESS_HUF) {
                    oname[strlen(oname) - 4] = 0;
                    strcat(oname,".lz");
                } else 
                    oname[strlen(oname) - 3] = 0;
                output = fopen(oname,"wb");
                comp_info.decompress(input,output,comp_mode - DECOMPRESS);
            }
        }
    }

    return 0;
}
