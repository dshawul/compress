#ifndef __COMPRESS__
#define __COMPRESS__

#include <stdio.h>

#include "my_types.h"

/*
Bit operations
*/
#define _byte_1   UINT64(0x00000000000000ff)
#define _byte_all UINT64(0xffffffffffffffff)
#define _byte_32  0xffffffff

#define addbits(x) {                                      \
    while(x > length) {                                   \
	   code = (code << 8) | *in++;                        \
	   length += 8;                                       \
	}                                                     \
};

#define getbits(x,v) {                                    \
    addbits(x)                                            \
	length -= x;                                          \
	v = UBMP32(code >> length) & (_byte_32 >> (32 - x));  \
};

/*
consts
*/

#define BLOCK_SIZE                         (1 << 13)
#define MAX_LEN                            32
#define INVALID                            -1

/*
Lempel Ziv consts
*/
#define DISTANCE_BITS                      12
#define LENGTH_BITS                        8
#define PAIR_BITS                          (DISTANCE_BITS + LENGTH_BITS)
#define LITERAL_BITS                       8
#define F_PAIR_BITS                        (PAIR_BITS + 1)
#define F_LITERAL_BITS                     (LITERAL_BITS + 1)

#define MIN_MATCH_LENGTH                   ((PAIR_BITS >> 3) + 1 + 1)
#define MAX_MATCH_LENGTH                   ((1 << LENGTH_BITS) - 1 + MIN_MATCH_LENGTH)

#define WINDOW_SIZE                        (1 << DISTANCE_BITS)

#define LENGTH_CODES                       30
#define DISTANCE_CODES                     31

#define EOB_MARKER                         (1 << LITERAL_BITS)
#define LENGTH_MARKER                      (EOB_MARKER + 1)
#define LITERAL_CODES                      ((1 << LITERAL_BITS) + LENGTH_CODES + 1)

/*
Mapping of distances.This greatly reduces space requirement 
for huffman trees.Distrances from 0-4096  are mapped to 0-23
*/
extern const int extra_dbits[];
extern const int base_dist[];
extern const int extra_lbits[];
extern const int base_length[];
/*
match length/position pair
*/
struct PAIR {
	int pos;
	int len;
	PAIR() {
		pos = 0;
		len = 0;
	}
};

/*
huffman tree
*/

struct NODE {
	int   symbol;
	UBMP32 freq;
	UBMP8  skip;
    NODE*  left;
	NODE*  right;
	NODE() {
		clear();
	}
	void clear() {
		symbol = INVALID;
		freq   = 0;
		skip   = 1;
		left   = 0;
		right  = 0;
	}
};

struct CANN {
    int   symbol;
	UBMP32 code;
	UBMP32 mask;
	UBMP8 length;
	CANN() {
		symbol = INVALID;
		code = 0;
		length = 0;
	}
};

struct HUFFMAN {
	NODE*   nodes;
	CANN*   cann;
	CANN*   pstart[MAX_LEN];
	UBMP8   min_length,
		    max_length;
	UBMP32  MAX_LEAFS;
	UBMP32  MAX_NODES;
	void clear_nodes();
	void build_cann_from_nodes();
	void build_cann_from_length();
	void print_all_node();
    void print_all_cann();
};
/*
file info
*/
class COMP_INFO {

public:
	FILE*   pf;
	UBMP32* index_table;
	UBMP32  size,
		    orgsize,
			cmpsize,
			n_blocks,
			block_size,
			read_start;
	HUFFMAN huffman;
	HUFFMAN huffman_pos;
    COMP_INFO();
public:
	bool open(FILE*);
	void compress(FILE*,FILE*);
	void decompress(FILE*,FILE*);
	void collect_frequency(UBMP8*,UBMP32);
	int encode(UBMP8*,UBMP8*,UBMP32);
	int decode(UBMP8*,UBMP8*,UBMP32);
	UBMP64 read_bytes(int);
};

#endif
