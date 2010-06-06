#include "codec.h"

const int extra_dbits[] = {
	0,  0,  0,  0,  1,  1,  2,  2,
	3,  3,  4,  4,  5,  5,  6,  6,
	7,  7,  8,  8,  9,  9, 10, 10,
   11, 11, 12, 12, 13, 13
};

const int base_dist[] = {
	1,    2,    3,    4,    5,    7,    9,   13, 
   17,   25,   33,   49,   65,   97,  129,  193, 
  257,  385,  513,  769, 1025, 1537, 2049, 3073,
 4097, 6145, 8193,12289,16385,24577,999999999 
};

const int extra_lbits[] = {
	0,  0,  0,  0,  0,  0,  0,  0,
	1,  1,  1,  1,  2,  2,  2,  2,
    3,  3,  3,  3,  4,  4,  4,  4, 
	5,  5,  5,  5,  0
};

const int base_length[] = {
	0,  1,  2,  3,  4,  5,  6,  7, 
    8, 10, 12, 14, 16, 20, 24, 28,
   32, 40, 48, 56, 64, 80, 96,112,
  128,160,192,224,255,999999999 
};

COMP_INFO::COMP_INFO() {
	huffman.MAX_LEAFS = LITERAL_CODES;
	huffman.MAX_NODES = 2 * huffman.MAX_LEAFS - 1;
	huffman_pos.MAX_LEAFS = DISTANCE_CODES;
	huffman_pos.MAX_NODES = 2 * huffman_pos.MAX_LEAFS - 1;
}
