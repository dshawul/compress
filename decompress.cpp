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
decode
*/
template<bool dolz>
int COMP_INFO::_decode(
				    const UBMP8* in_table,
		            UBMP8* out_table,
					const UBMP32 size
				 ) {

	const UBMP8* in = in_table;
	const UBMP8* ine = in_table + size;
	UBMP8* out = out_table;
	UBMP8* ptr;
	PAIR pair;

	UBMP16 j,extra;
	UBMP32 v;
	CANN  *pcann;
	HUFFMAN* phuf;
	int diff;

	BITSET bs(in,out);
	BITSET bso(in,out);

#define HUFFMAN_DECODE(huf,x) {                                                     \
	phuf = &huf;                                                                    \
	bs.addbits(phuf->max_length);                                                   \
	for(j = phuf->min_length; j <= phuf->max_length;j++) {                          \
		if(!(pcann = phuf->pstart[j]))                                              \
			continue;                                                               \
		diff = UBMP32(bs.read(j) & pcann->mask) - pcann->code;						\
		if(diff >= 0) {                                                             \
			x = (pcann + diff)->symbol;                                             \
			bs.trim(j);																\
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
			if(extra != 0)
				pair.len += bs.getbits(extra);
			pair.len += MIN_MATCH_LENGTH;

			//distance
			HUFFMAN_DECODE(huffman_pos,v);

			pair.pos = base_dist[v];
			extra = extra_dbits[v];
			if(extra != 0)
				pair.pos += bs.getbits(extra);

			//copy bytes
			if(dolz) {
				bso.writepair(pair);
			} else {
				ptr = out - pair.pos;
				for(int i = 0; i < pair.len;i++)
					*out++ = *ptr++;
			}
		} else {
			//write literal
			if(dolz)
				bso.writeliteral(v);
			else
				*out++ = (UBMP8)v;
		}
		if(dolz)
			bso.writebits();
	}
	if(dolz)
		bso.flushbits();

	return int(out - out_table);
}
/*
decode
*/
int COMP_INFO::decode_huff(
				    const UBMP8* in_table,
		            UBMP8* out_table,
					const UBMP32 size
				 ) {
	return _decode<true>(in_table,out_table,size);
}
/*
decode
*/
int COMP_INFO::decode(
				    const UBMP8* in_table,
		            UBMP8* out_table,
					const UBMP32 size
				 ) {
	return _decode<false>(in_table,out_table,size);
}
/*
decode lz
*/
int COMP_INFO::decode_lz(
				    const UBMP8* in_table,
		            UBMP8* out_table,
					const UBMP32 size
				 ) {

	const UBMP8* in = in_table;
	const UBMP8* ine = in_table + size;
	UBMP8* out = out_table;
	UBMP8* ptr;
	BITSET bs(in,out);
	PAIR pair;
	UBMP32 v;

    while(in < ine) {
		v = bs.getbits(1);
        if(v == 1) {
			v = bs.getbits(PAIR_BITS);
			pair.len = (v >> DISTANCE_BITS);
			pair.pos = (v & (_byte_32 >> (32 - DISTANCE_BITS)));
			pair.len += MIN_MATCH_LENGTH;
			//copy bytes
			ptr = out - pair.pos;
			for(int i = 0; i < pair.len;i++)
				*out++ = *ptr++;
		} else {
			v = bs.getbits(LITERAL_BITS);
			//literal byte
			*out++ = (UBMP8)v;
		}
	}
	return int(out - out_table);
}
/*
open encoded file
*/
bool COMP_INFO::open(FILE* myf,int type) {
    UBMP32 i;
	
	pf = myf;

	//read counts
	fread(&orgsize,4,1,pf);
	fread(&cmpsize,4,1,pf);
	fread(&n_blocks,4,1,pf);
	fread(&block_size,4,1,pf);
#ifdef BIGENDIAN
	bswap32(orgsize);
	bswap32(cmpsize);
	bswap32(n_blocks);
	bswap32(block_size);
#endif

	//skip reserve bytes
	fseek(pf,40,SEEK_CUR);

	if(type <= 1) {
		//huffman tree
		huffman.cann = new CANN[huffman.MAX_LEAFS];
		huffman_pos.cann = new CANN[huffman_pos.MAX_LEAFS];
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
		//build cannoncial huffman
		huffman.build_cann_from_length();
		huffman_pos.build_cann_from_length();
	}

	//read index table
	index_table = new UBMP32[n_blocks + 1];
	fread(index_table,4,(n_blocks+1),pf);
#ifdef BIGENDIAN
	for(i = 0; i < int(n_blocks + 1);i++)
		bswap32(index_table[i]);
#endif

	read_start = ftell(pf);

	return true;
}
/*
decompress file
*/
#include <time.h>

#ifndef CLOCKS_PER_SEC
  #define CLOCKS_PER_SEC CLK_TCK
#endif

void COMP_INFO::decompress(FILE* inf,FILE* outf, int type) {
	UBMP8 buffer[2 * BLOCK_SIZE];
	UBMP8 buffer2[2 * BLOCK_SIZE];
	UBMP32 i,buffer_size;

	//open compressed file
	open(inf,type);

	//time
	clock_t start_t, end_t;
	double seconds = 0.0,dseconds = 0.0;
	start_t = clock();

	//header when writing to LZ file
	if(type == 1) {
		//write counts
		fwrite(&orgsize,4,1,outf);
		fwrite(&cmpsize,4,1,outf);
		fwrite(&n_blocks,4,1,outf);
		fwrite(&block_size,4,1,outf);

		//skip reserve bytes
		fseek(outf,40,SEEK_CUR);

		//index table
		fwrite(index_table,4,n_blocks + 1,outf);
	}

	//decompress each block
	size = 0;
    for(i = 0; i < n_blocks;i++) {
		buffer_size = index_table[i + 1] - index_table[i];

		fread(&buffer,1,buffer_size,pf);
		end_t = clock();
		if(type == 0)
			buffer_size = decode(buffer,buffer2,buffer_size);
		else if(type == 1)
			buffer_size = decode_huff(buffer,buffer2,buffer_size);
		else
			buffer_size = decode_lz(buffer,buffer2,buffer_size);
		dseconds += (double)(clock() - end_t) / CLOCKS_PER_SEC;
		fwrite(&buffer2,1,buffer_size,outf);

		index_table[i] = size;
		size += buffer_size;
	}
	index_table[i] = size;

	//rewrite index table
	if(type == 1) {
		fseek(outf,56,SEEK_SET);
		fwrite(index_table,4,n_blocks + 1,outf);
	}

	//time
	end_t = clock();
    seconds = (double)(end_t - start_t) / CLOCKS_PER_SEC;
	printf ("%g sec total, %g sec decompressing, %d Mb file, %g Mb/sec, %g Mb/sec\n", 
		seconds,dseconds, size/(1024*1024), 
		(size/(1024*1024))/seconds, (size/(1024*1024))/dseconds);
}
/*
end
*/
