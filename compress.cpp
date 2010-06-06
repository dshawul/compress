#include "codec.h"
#include <cstring>
#include <cstdlib>

/*
distance code
*/
#define d_code(d,x) {                  \
	d = 0;                             \
    while(x >= base_dist[d]) d++;      \
    d--;                               \
}

#define l_code(d,x) {                  \
	d = 0;                             \
    while(x >= base_length[d]) d++;    \
    d--;                               \
}
/*
simple linked list to accelerate searching matches
*/
struct LINK {
	UBMP8* val;
	LINK* next;

	void add(UBMP8* c) {
		free_link->val = val;
		free_link->next = next;
		val = c;
		next = free_link;
		free_link++;
	}
	void remove() {
		next = 0;
		val = 0;
	}
	static void clear();

	static LINK* free_link;
	static LINK list[256];
    static LINK list_block[BLOCK_SIZE];
};

LINK* LINK::free_link;
LINK  LINK::list[256];
LINK  LINK::list_block[BLOCK_SIZE];

void LINK::clear() {
	for(int i = 0;i < 256;i++)
		list[i].remove();
	free_link = list_block;
}
/*
find match
*/
static PAIR find_match(UBMP8* in,UBMP8* ine) {

	UBMP8 *swindow,*lahead;
	PAIR pairm,pair;
	
	LINK* current = &LINK::list[*in];
	while(current && current->val) {
		
		//get position
		swindow = current->val;
		pair.pos = in - swindow;
		
		if(pair.pos >= WINDOW_SIZE)
			break;
		
		//determine match length
		lahead = in;
		while(*swindow == *lahead
			&& lahead != ine
			&& lahead != in + MAX_MATCH_LENGTH
			) {
			swindow++;
			lahead++;
		} 
		pair.len = lahead - in;
		
		//prefer long and closer matches
		if(pair.len >= MIN_MATCH_LENGTH
			&& (pair.len > pairm.len || (pair.len == pairm.len && pair.pos < pairm.pos))
			) {
			pairm.pos = pair.pos;
			pairm.len = pair.len;
		}
		if(pairm.len >= MAX_MATCH_LENGTH)
			break;
		
		//next position
		current = current->next;
	}
	return pairm;
}
/*
encode using lempel ziv algorithm.
*/
static UBMP32 lz_encode(UBMP8* in_table,UBMP8* out_table,UBMP32 size) {

	register int i;
	UBMP8* in = in_table;
	UBMP8* ine = in_table + size;
	UBMP8* swindow = in_table;
	UBMP8* out = out_table;
	PAIR pair;
	PAIR pair1;

	UBMP8 length = 0;
	UBMP64 code = 0;
	UBMP32 temp_code = 0;
	
	LINK::clear();

	while(in < ine) {
	    pair = find_match(in,ine);

		if(pair.len) {

            //lazy match
			i = 0;
			do {
				pair1 = find_match(in + 1,ine);
				
				if(pair1.len > pair.len) {
					pair = pair1;
					
					temp_code = (0 << LITERAL_BITS) | (*in);
					code = ((code << F_LITERAL_BITS) | temp_code);
					length += F_LITERAL_BITS;
					
					LINK::list[*in].add(in);
					in++;
				} else {
					break;
				}
				i++;
			} while(in < ine && i < pair.len);

            //end
			temp_code = (1 << PAIR_BITS) | ((pair.len - MIN_MATCH_LENGTH) << DISTANCE_BITS) | pair.pos;
			code = ((code << F_PAIR_BITS) | temp_code);
			length += F_PAIR_BITS;

			for(i = 0;i < pair.len;i++) {
				LINK::list[*in].add(in);
				in++;
			}

		} else {

			temp_code = (0 << LITERAL_BITS) | (*in);
			code = ((code << F_LITERAL_BITS) | temp_code);
			length += F_LITERAL_BITS;

			LINK::list[*in].add(in);
			in++;
		}

		while(length >= 8) {
			length -= 8;
			*out++ = UBMP8((code >> length) & _byte_1);
			code &= (_byte_all >> (64 - length));
		}
	}
	if(length) {
		length -= 8;
		*out++ = UBMP8((code  << (-length)) & _byte_1);
	}

	return (out - out_table);
}
/*
Huffman
*/
static const UBMP32 MAX_FRQ  = UBMP32(~0);

static void find_least(NODE* nodes, const int size,int& min1,int& min2) {

	UBMP32 min1v,min2v,value;
	min1  = INVALID;
	min2  = INVALID;
	min1v = MAX_FRQ;
	min2v = MAX_FRQ;
	for(int i = 0; i < size;i++) {
		if(nodes[i].skip)
			continue;
		value = nodes[i].freq;
		if(value < min1v) {
			min2 = min1;
			min2v = min1v;
			min1 = i;
			min1v = value;
		} else if(value < min2v) {
			min2 = i;
			min2v = value;
		}
	}
}
static void assign_depth(NODE* node,CANN* cann,UBMP8 depth) {
	if(node->symbol != INVALID)
	    cann[node->symbol].length = depth;
	if(node->left) {
		assign_depth(node->left,cann,depth + 1);
	}
    if(node->right) {
		assign_depth(node->right,cann,depth + 1);
	}
	
}
void HUFFMAN::build_cann_from_nodes() {

	CANN tempcann;
	UBMP32 i,j;
	int min1,min2,temp;
    NODE* root;

DOAGAIN:

    root = 0;
    for(i = MAX_LEAFS; i < MAX_NODES;i++) {
		find_least(nodes,i,min1,min2);
		
		if(min1 == INVALID || min2 == INVALID)
			break;

		nodes[min1].skip = 1;
		nodes[min2].skip = 1;
		nodes[i].freq = nodes[min1].freq + nodes[min2].freq;
		nodes[i].left = &nodes[min1];
        nodes[i].right = &nodes[min2];
		nodes[i].skip = 0;
		root = &nodes[i];
	}

	//take care of one node tree
	if(!root) {
		for(i = 0; i < MAX_LEAFS;i++) {
			if(nodes[i].freq) {
				root = &nodes[i];
				break;
			}
		}
		if(!root) {
			return;
		}
	}

	//init cannonical code
	for(i = 0;i < MAX_LEAFS;i++) {
		cann[i].symbol = i;
		cann[i].length = 0;
		cann[i].code = 0;
	}

	//assign length
	if(!root->left && !root->right) {
		cann[root->symbol].length = 1;
	} else {
  	    assign_depth(root,cann,0);
	}
	
	//limit depth to MAX_LEN bits
	int temper = false;
	for(i = 0;i < MAX_LEAFS;i++) {
		if(cann[i].length >= MAX_LEN) {
			temper = true;
			break;
		}
	}
	if(temper) {
		for(i = 0; i < MAX_LEAFS; i++) {
			if(nodes[i].freq) {
				nodes[i].freq = (nodes[i].freq + 1) >> 1;
			}
			nodes[i].skip = 0;
		}
		goto DOAGAIN;
	}

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
	UBMP16 length = cann[MAX_LEAFS - 1].length;

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

	//sort by symbol
	for(i = 0;i < MAX_LEAFS;i++) {
		for(j = i + 1;j < MAX_LEAFS;j++) {
			temp = cann[j].symbol - cann[i].symbol;
			if(temp < 0) {
				tempcann = cann[j];
				cann[j] = cann[i];
                cann[i] = tempcann;
			}
		}
	}
}
void HUFFMAN::clear_nodes() {
	for(UBMP32 i = 0;i < MAX_LEAFS;i++) {
		nodes[i].clear();
	}
}
/*
Debugging
*/
static void display(int b,int length) {
	char temp[256];
	strcpy(temp,""); 
	for(int i = 31 ;i >= 0; i--) {
		if(i > length - 1) strcat(temp," ");
		else if(b & (1 << i)) strcat(temp,"1");
        else strcat(temp,"0");
	}
    printf(temp);
}

static void print_node(NODE* node) {
	printf("0x%03X %10d\n",node->symbol,node->freq);
}

static void print_cann(CANN* cann) {
	printf("0x%03X ",cann->symbol);
	display(cann->code,cann->length);
	printf(" %2d\n",cann->length);
}

void HUFFMAN::print_all_node() {
	for(UBMP32 i = 0; i < MAX_LEAFS; i++) {
		if(nodes[i].freq) {
			print_node(&nodes[i]);
		}
	}
}

void HUFFMAN::print_all_cann() {
	for(UBMP32 i = 0;i < MAX_LEAFS;i++) {
		if(cann[i].length) {
			print_cann(&cann[i]);
		}
	}
}
/*
collect statistics
*/
void COMP_INFO::collect_frequency(UBMP8* in_table,UBMP32 size) {

	UBMP8* in = in_table;
	UBMP8* ine = in_table + size;
	UBMP8  length = 0;
	UBMP64 code = 0;
	PAIR pair;
	UBMP32 v,d;

    while(in < ine) {
		getbits(1,v);
        if(v == 1) {
			getbits(PAIR_BITS,v);
			pair.len = (v >> DISTANCE_BITS);
			pair.pos = (v & (_byte_32 >> (32 - DISTANCE_BITS)));

			l_code(d,pair.len);
			d += LENGTH_MARKER;
			huffman.nodes[d].symbol = d;
			huffman.nodes[d].freq++;
		    huffman.nodes[d].skip = 0;
			
			d_code(d,pair.pos);
			huffman_pos.nodes[d].symbol = d;
			huffman_pos.nodes[d].freq++;
			huffman_pos.nodes[d].skip = 0;
            
		} else {
			getbits(LITERAL_BITS,v);
			huffman.nodes[v].symbol = v;
			huffman.nodes[v].freq++;
		    huffman.nodes[v].skip = 0;
		}
	}
}
/*
encode
*/
int COMP_INFO::encode(
				    UBMP8* in_table,
		            UBMP8* out_table,
					UBMP32 size
				 ) {

	UBMP8* in = in_table;
	UBMP8* ine = in_table + size;
	UBMP8* out = out_table;

	UBMP8 length = 0;
	UBMP64 code = 0;

	UBMP8 o_length = 0;
	UBMP64 o_code = 0;

	PAIR pair;
	UBMP32 v,d;

	int extra;

	CANN  *pcann;

    while(in < ine) {
		getbits(1,v);
        if(v == 1) {
			getbits(PAIR_BITS,v);
			pair.len = (v >> DISTANCE_BITS);
			pair.pos = (v & (_byte_32 >> (32 - DISTANCE_BITS)));

			//length
			l_code(d,pair.len);
			pcann = &huffman.cann[d + LENGTH_MARKER];
			o_code = ((o_code << pcann->length) | pcann->code);
			o_length += pcann->length;

			extra = extra_lbits[d];
			if (extra != 0) {
				o_code = ((o_code << extra) | (pair.len - base_length[d]));
				o_length += extra;
			}

			//distance
			d_code(d,pair.pos);
			pcann = &huffman_pos.cann[d];
			o_code = ((o_code << pcann->length) | pcann->code);
			o_length += pcann->length;

			extra = extra_dbits[d];
			if (extra != 0) {
				o_code = ((o_code << extra) | (pair.pos - base_dist[d]));
				o_length += extra;
			}
			
		} else {
			getbits(LITERAL_BITS,v);

            //literal
			pcann = &huffman.cann[v];
			o_code = ((o_code << pcann->length) | pcann->code);
			o_length += pcann->length;
		}

		//output
		while(o_length >= 8) {
			o_length -= 8;
			*out++ = UBMP8((o_code >> o_length) & _byte_1);
			o_code &= (_byte_all >> (64 - o_length));
		}
	}

	//end of block marker
	pcann = &huffman.cann[EOB_MARKER];
	o_code = ((o_code << pcann->length) | pcann->code);
	o_length += pcann->length;

	//flush everything b/s we are going to finish
	while(o_length >= 8) {
		o_length -= 8;
		*out++ = UBMP8((o_code >> o_length) & _byte_1);
		o_code &= (_byte_all >> (64 - o_length));
	}

	if(o_length) {
		o_length -= 8;
		*out++ = UBMP8((o_code  << (-o_length)) & _byte_1);
	}

	return (out - out_table);
}
/*
compress
*/
void COMP_INFO::compress(char* inf_name,char* ouf_name) {
	FILE *inf;
	UBMP8 *in_table,
		  *out_table,
		  *ptr;
	UBMP32 i,j,buffer_size,index_start;
	BMP32 temp;

	//open file
	inf = fopen(inf_name,"rb");
	 
	//get size of file and allocate buffers
	fseek(inf,0,SEEK_END);
	orgsize = ftell(inf);
	n_blocks = (orgsize / BLOCK_SIZE) + ((orgsize % BLOCK_SIZE) != 0);
	size = (n_blocks * BLOCK_SIZE);
    fseek(inf,0,SEEK_SET);

	//allocate tables
	in_table    = new UBMP8[2 * BLOCK_SIZE];
	out_table   = new UBMP8[2 * BLOCK_SIZE];
	index_table = new UBMP32[n_blocks + 1];
	huffman.cann = new CANN[huffman.MAX_LEAFS];
	huffman.nodes = new NODE[huffman.MAX_NODES];
	huffman_pos.cann = new CANN[huffman_pos.MAX_LEAFS];
	huffman_pos.nodes = new NODE[huffman_pos.MAX_NODES];

	//start compressing
	printf("compressing %s\n",inf_name);

	/*
    compress using Lempel Ziv
	*/
	char temp_name[512];
	strcpy(temp_name,ouf_name);
	strcat(temp_name,".lz");

	pf = fopen(temp_name,"wb");

	cmpsize = 0;
	for(i = 0; i < n_blocks;i++) {
		buffer_size = (i == n_blocks - 1) ? (orgsize - i * BLOCK_SIZE) : BLOCK_SIZE;
		index_table[i] = cmpsize;

		ptr = in_table;
		for(j = 0;j < buffer_size;j++)
			*ptr++ = (UBMP8) fgetc(inf);
		
		buffer_size = lz_encode(in_table,out_table,buffer_size);
		collect_frequency(out_table,buffer_size);

		for(j = 0;j < buffer_size;j++)
			fputc(out_table[j],pf);

		cmpsize += buffer_size;
	}
	index_table[i] = cmpsize;

	//end of block marker
	huffman.nodes[EOB_MARKER].symbol = EOB_MARKER;
	huffman.nodes[EOB_MARKER].freq = 1;
    huffman.nodes[EOB_MARKER].skip = 0;

	fclose(pf);
	fclose(inf);

	/*
	build cannonical huffman code
	*/
	huffman.build_cann_from_nodes();
	huffman_pos.build_cann_from_nodes();

	/*
	write header of file
	*/
	pf = fopen(ouf_name,"wb");

	//write count
	fwrite(&orgsize,4,1,pf);
	fwrite(&cmpsize,4,1,pf);
    fwrite(&n_blocks,4,1,pf);
	block_size = BLOCK_SIZE;
	fwrite(&block_size,4,1,pf);

	//write reserve bytes
	for(i = 0;i < 10;i++)
	   fwrite(&temp,4,1,pf);

	//write length
	for(i = 0; i < huffman.MAX_LEAFS; i++)
		fwrite(&huffman.cann[i].length,1,1,pf);

	//write length
	for(i = 0; i < huffman_pos.MAX_LEAFS; i++)
		fwrite(&huffman_pos.cann[i].length,1,1,pf);

	index_start = ftell(pf);

	//write index table
	for(i = 0; i < n_blocks + 1;i++)
		fwrite(&index_table[i],4,1,pf);

	read_start = ftell(pf);

	/*
	compress using huffman
	*/
	inf = fopen(temp_name,"rb");

	cmpsize = 0;
	for(i = 0; i < n_blocks;i++) {
		buffer_size = index_table[i + 1] - index_table[i];
		index_table[i] = cmpsize;

		ptr = in_table;
		for(j = 0;j < buffer_size;j++)
			*ptr++ = (UBMP8) fgetc(inf);

		buffer_size = encode(in_table,out_table,buffer_size);

		for(j = 0;j < buffer_size;j++)
			fputc(out_table[j],pf);
		
		cmpsize += buffer_size;
	}
	index_table[i] = cmpsize;

	/*
	write updated index info
	*/
	fseek(pf,index_start,SEEK_SET);

	for(i = 0; i < n_blocks + 1;i++)
		fwrite(&index_table[i],4,1,pf);

	/*
	close files and delete temporary file
	*/
	fclose(inf);
	fclose(pf);

	char command[512];
#ifdef _MSC_VER
	sprintf(command,"del %s",temp_name);
#else
	sprintf(command,"rm -r %s",temp_name);
#endif
	system(command);

	/*
	free memory
	*/
	delete[] in_table;
	delete[] out_table;
	delete[] index_table;
	delete[] huffman.cann;
	delete[] huffman.nodes;
	delete[] huffman_pos.cann;
	delete[] huffman_pos.nodes;
}
