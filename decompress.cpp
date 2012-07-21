#include "codec.h"

/*
Huffman
*/
void HUFFMAN::build_cann_from_length() {

	UBMP32 i,j;
	int temp;
	CANN tempcann;

	//sort by length
	for(i = 0;i < MAX_LEAFS;i++) {
		for(j = i + 1;j < MAX_LEAFS;j++) {
			temp = cann[j].length - cann[i].length;
			if(temp == 0) {
				temp = cann[j].symbol - cann[i].symbol;
				temp = -temp;
			}
			if(temp < 0) {
				tempcann = cann[j];
				cann[j] = cann[i];
                cann[i] = tempcann;
			}
		}
	}

    //assign code
	UBMP32 code   = cann[MAX_LEAFS - 1].code;
	UBMP8 length = cann[MAX_LEAFS - 1].length;

	for(int k = MAX_LEAFS - 2; k >= 0; k--) {
        if (cann[k].length == 0) {
            break;
        }
		if(cann[k].length != length) {
			code >>= (length - cann[k].length);
			length = cann[k].length;
		}
		code++;
		cann[k].code = code;
    }

	//sort equal lengths lexically
	for(i = 0;i < MAX_LEAFS;i++) {
		for(j = i + 1;j < MAX_LEAFS;j++) {
			temp = cann[j].length - cann[i].length;
			if(temp == 0) {
				temp = cann[j].symbol - cann[i].symbol;
			}
			if(temp < 0) {
				tempcann = cann[j];
				cann[j] = cann[i];
                cann[i] = tempcann;
			}
		}
	}

	//mark start of each length
    for (i = 0; i < MAX_LEN; i++)
        pstart[i] = 0;

	min_length = MAX_LEN;
	max_length = 0;
	length = 0;
	for (i = 0; i < MAX_LEAFS; i++) {

        if (cann[i].length > length) {
			length = cann[i].length;
			pstart[length] = &cann[i];

			if(length < min_length)
				min_length = length;
			if(length > max_length)
			    max_length = length;
        }
    }
}

/*
read n bytes assuming little endian byte order
*/
UBMP64 COMP_INFO::read_bytes(int count) {
    UBMP64 x = 0;
	UBMP8* c = (UBMP8*) &x;
	for(int i = 0; i < count; i++) {
		c[i] = ((UBMP8) fgetc(pf));
	}
	return x;
}
/*
decode
*/
int COMP_INFO::decode(
				    UBMP8* in_table,
		            UBMP8* out_table,
					UBMP32 size
				 ) {

	UBMP8* in = in_table;
	UBMP8* ine = in_table + size;
	UBMP8* out = out_table;
	UBMP8* ptr;
	PAIR pair;

	UBMP16  length = 0,j,extra;
	UBMP64 code = 0;

	UBMP32 v,temp;
	int diff;
	CANN  *pcann;
	HUFFMAN* phuf;

#define HUFFMAN_DECODE(huf,x) {                                                     \
	phuf = &huf;                                                                    \
	addbits(phuf->max_length);                                                      \
	for(j = phuf->min_length; j <= phuf->max_length;j++) {                          \
		if(!(pcann = phuf->pstart[j]))                                              \
			continue;                                                               \
		diff = UBMP32((code >> (length - j)) & pcann->mask) - pcann->code;          \
		if(diff >= 0) {                                                             \
			x = (pcann + diff)->symbol;                                             \
			length -= j;                                                            \
			break;                                                                  \
		}                                                                           \
	}                                                                               \
};

	while(in < ine) {

		HUFFMAN_DECODE(huffman,v);

		if(v == EOB_MARKER)
			break;

        if(v > EOB_MARKER) {

			//length
			v -= LENGTH_MARKER;
            pair.len = base_length[v];
			extra = extra_lbits[v];
			if(extra != 0) {
				getbits(extra,temp);
				pair.len += temp;
			}
			pair.len += MIN_MATCH_LENGTH;

			//distance
			HUFFMAN_DECODE(huffman_pos,v);

			pair.pos = base_dist[v];
			extra = extra_dbits[v];
			if(extra != 0) {
				getbits(extra,temp);
				pair.pos += temp;
			}

			ptr = out - pair.pos;
			for(int i = 0; i < pair.len;i++)
				*out++ = *ptr++;

		} else {
			*out++ = (UBMP8)v;
		}
	}

	return (out - out_table);
}
/*
open encoded file
*/
bool COMP_INFO::open(FILE* myf) {
    UBMP32 i;
	
	pf = myf;

	//open file
	huffman.cann = new CANN[huffman.MAX_LEAFS];
	huffman_pos.cann = new CANN[huffman_pos.MAX_LEAFS];

	//read counts
	orgsize = (UBMP32) read_bytes(4);
	cmpsize = (UBMP32) read_bytes(4);
	n_blocks = (UBMP32) read_bytes(4);
	block_size = (UBMP32) read_bytes(4);

	//read reserve bytes
	for(i = 0;i < 10;i++) {
	   read_bytes(4);
	}

	//read length
	for(i = 0; i < huffman.MAX_LEAFS; i++) {
		fread(&huffman.cann[i].length,1,1,pf);
		huffman.cann[i].symbol = i;
		huffman.cann[i].code = 0;
		huffman.cann[i].mask = (1 << huffman.cann[i].length) - 1;
	}

	//read length
	for(i = 0; i < huffman_pos.MAX_LEAFS; i++) {
		fread(&huffman_pos.cann[i].length,1,1,pf);
		huffman_pos.cann[i].symbol = i;
		huffman_pos.cann[i].code = 0;
		huffman_pos.cann[i].mask = (1 << huffman_pos.cann[i].length) - 1;
	}
		
	//read index table
	index_table = new UBMP32[n_blocks + 1];
	for(i = 0; i < int(n_blocks + 1);i++)
		index_table[i] = (UBMP32) read_bytes(4);

	read_start = ftell(pf);

	huffman.build_cann_from_length();
    huffman_pos.build_cann_from_length();

	return true;
}
/*
decompress file
*/
void COMP_INFO::decompress(FILE* inf,FILE* outf) {
	UBMP32 i,start;
	UBMP8 buffer[2 * BLOCK_SIZE];
	UBMP8 buffer2[2 * BLOCK_SIZE];
	UBMP32 buffer_size;
	UBMP32 uncomp_size;

	//open compressed file
	open(inf);

	//decompress each block
    for(i = 0; i < n_blocks;i++) {
	
		if(i == n_blocks - 1) uncomp_size = orgsize - i * BLOCK_SIZE;
		else uncomp_size = BLOCK_SIZE;
		size += uncomp_size;

		buffer_size = index_table[i + 1] - index_table[i];

		start = read_start + index_table[i];
		fseek(pf,start,SEEK_SET);
		fread(&buffer,buffer_size,1,pf);

		buffer_size = decode(buffer,buffer2,buffer_size);

		for(UBMP32 k = 0;k < uncomp_size;k++)
		   fputc(buffer2[k],outf);
	}
}
/*
end
*/
