#include <cstring>
#include <cstdlib>
#include <vector>
#include <list>
#include <math.h>

#include "codec.h"

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
    const UBMP8* val;
    LINK* next;

    void add(const UBMP8* c) {
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
static PAIR find_match(const UBMP8* in,const UBMP8* ine) {

    const UBMP8 *swindow,*lahead;
    PAIR pairm,pair;
    
    LINK* current = &LINK::list[*in];
    while(current && current->val) {
        
        //get position
        swindow = current->val;
        pair.pos = int(in - swindow);
        
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
        pair.len = int(lahead - in);
        
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
int COMP_INFO::encode_lz(
                    const UBMP8* in_table,
                    UBMP8* out_table,
                    const UBMP32 size
                 ) {

    register int i;
    const UBMP8* in = in_table;
    const UBMP8* ine = in_table + size;
    const UBMP8* swindow = in_table;
    UBMP8* out = out_table;
    PAIR pair;
    PAIR pair1;
    BITSET bs(in,out);
    LINK::clear();

    while(in < ine) {
        pair = find_match(in,ine);

        if(pair.len) {
            //lazy match
            i = 0;
            do {
                if(pair.len >= MAX_MATCH_LENGTH)
                    break;

                pair1 = find_match(in + 1,ine);

                if(pair1.len > pair.len) {
                    pair = pair1;

                    //write literal byte
                    bs.writeliteral(*in);

                    LINK::list[*in].add(in);
                    in++;

                    bs.writebits();
                } else {
                    break;
                }
                i++;
            } while(in < ine && i < pair.len);

            //write pair bytes
            bs.writepair(pair);

            for(i = 0;i < pair.len;i++) {
                LINK::list[*in].add(in);
                in++;
            }
        } else {
            //write literal byte
            bs.writeliteral(*in);

            LINK::list[*in].add(in);
            in++;
        }
        bs.writebits();
    }
    bs.flushbits();

    return int(out - out_table);
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
collect statistics
*/
void COMP_INFO::collect_frequency(const UBMP8* in_table,const UBMP32 size) {

    const UBMP8* in = in_table;
    const UBMP8* ine = in_table + size;
    UBMP8* out;
    BITSET bs(in,out);
    PAIR pair;
    UBMP32 v,d;

    while(in < ine) {
        v = bs.getbits(1);
        if(v == 1) {
            v = bs.getbits(PAIR_BITS);
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
            v = bs.getbits(LITERAL_BITS);
            huffman.nodes[v].symbol = v;
            huffman.nodes[v].freq++;
            huffman.nodes[v].skip = 0;
        }
    }
}
/*
encode
*/
int COMP_INFO::encode_huff(
                    const UBMP8* in_table,
                    UBMP8* out_table,
                    const UBMP32 size
                 ) {

    const UBMP8* in = in_table;
    const UBMP8* ine = in_table + size;
    UBMP8* out = out_table;
    BITSET bs(in,out);
    BITSET bso(in,out);
    PAIR pair;
    UBMP32 v,d;
    int extra;
    CANN  *pcann;

    while(in < ine) {
        v = bs.getbits(1);
        if(v == 1) {
            v = bs.getbits(PAIR_BITS);
            pair.len = (v >> DISTANCE_BITS);
            pair.pos = (v & (_byte_32 >> (32 - DISTANCE_BITS)));
            //length
            l_code(d,pair.len);
            pcann = &huffman.cann[d + LENGTH_MARKER];
            bso.write(pcann->code,pcann->length);

            extra = extra_lbits[d];
            if (extra != 0)
                bso.write(pair.len - base_length[d],extra);

            //distance
            d_code(d,pair.pos);
            pcann = &huffman_pos.cann[d];
            bso.write(pcann->code,pcann->length);

            extra = extra_dbits[d];
            if (extra != 0)
                bso.write(pair.pos - base_dist[d],extra);
        } else {
            v = bs.getbits(LITERAL_BITS);
            //literal
            pcann = &huffman.cann[v];
            bso.write(pcann->code,pcann->length);
        }
        //write all bits
        bso.writebits();
    }

    //end of block marker
    pcann = &huffman.cann[EOB_MARKER];
    bso.write(pcann->code,pcann->length);

    //finish
    bso.writebits();
    bso.flushbits();

    return int(out - out_table);
}
/***********************************
 * compress
 **********************************/
void COMP_INFO::compress(FILE* inputf,FILE* outputf,int type) {
    UBMP8 *in_table,*out_table;
    UBMP32 i,buffer_size,index_start;

    //allocate tables
    in_table    = new UBMP8[2 * BLOCK_SIZE];
    out_table   = new UBMP8[2 * BLOCK_SIZE];
    huffman.cann = new CANN[huffman.MAX_LEAFS];
    huffman.nodes = new NODE[huffman.MAX_NODES];
    huffman_pos.cann = new CANN[huffman_pos.MAX_LEAFS];
    huffman_pos.nodes = new NODE[huffman_pos.MAX_NODES];

    /**********************************************************
     * Compress using Lempel Ziv and collect frequencey of symbols
     **********************************************************/
    char temp_name[512];
    sprintf(temp_name,"%s.lz",output_fname);

    std::list<UBMP32> lindex;
    {
        if(type == 0) {
            pf = fopen(temp_name,"wb");
        } else {
            pf = outputf;
            fseek(inputf,0,SEEK_END);
            UBMP32 TB_SIZE = ftell(inputf);
            fseek(inputf,0,SEEK_SET);
            n_blocks = int(ceil(double(TB_SIZE) / BLOCK_SIZE));
            fseek(pf,56 + (n_blocks + 1) * 4,SEEK_SET);
        }

        bool last = false;
        n_blocks = 0;
        cmpsize = 0;
        orgsize = 0;
        while(!last) {
            lindex.push_back(cmpsize);

            buffer_size = (UBMP32)fread(in_table,1,BLOCK_SIZE,inputf);
            if(buffer_size < BLOCK_SIZE)
                last = true;
            orgsize += buffer_size;

            buffer_size = encode_lz(in_table,out_table,buffer_size);
            if(type == 0)
                collect_frequency(out_table,buffer_size);
            
            fwrite(out_table,1,buffer_size,pf);

            cmpsize += buffer_size;
            n_blocks++;
        }
        lindex.push_back(cmpsize);

        if(type == 0)
            fclose(pf);
        else
            rewind(pf);
    }
    std::vector<UBMP32> vindex(lindex.begin(), lindex.end());
    lindex.clear();
    
    if(type == 0) {
        //end of block marker
        huffman.nodes[EOB_MARKER].symbol = EOB_MARKER;
        huffman.nodes[EOB_MARKER].freq = 1;
        huffman.nodes[EOB_MARKER].skip = 0;

        //build cannonical huffman code
        huffman.build_cann_from_nodes();
        huffman_pos.build_cann_from_nodes();
    }

    /************************
     * write header of file
     ************************/
    //write count
    fwrite(&orgsize,4,1,outputf);
    fwrite(&cmpsize,4,1,outputf);
    fwrite(&n_blocks,4,1,outputf);
    block_size = BLOCK_SIZE;
    fwrite(&block_size,4,1,outputf);

    //write reserve bytes
    fseek(outputf,40,SEEK_CUR);

    //write lengths
    if(type == 0) {
        for(i = 0;i < huffman.MAX_LEAFS;i++)
            fwrite(&huffman.cann[i].length,1,1,outputf);
        for(i = 0;i < huffman_pos.MAX_LEAFS;i++)
            fwrite(&huffman_pos.cann[i].length,1,1,outputf);
    }

    //write index table
    index_start = ftell(outputf);
    fwrite(&vindex[0],4,n_blocks + 1,outputf);

    /***********************************
     * Compress using huffman
     ***********************************/
    if(type == 0)
    {
        FILE* inf = fopen(temp_name,"rb");

        cmpsize = 0;
        for(i = 0; i < n_blocks;i++) {
            buffer_size = vindex[i + 1] - vindex[i];
            vindex[i] = cmpsize;

            fread(in_table,1,buffer_size,inf);
            buffer_size = encode_huff(in_table,out_table,buffer_size);
            fwrite(out_table,1,buffer_size,outputf);

            cmpsize += buffer_size;
        }
        vindex[i] = cmpsize;

        //write updated index info
        fseek(outputf,index_start,SEEK_SET);
        fwrite(&vindex[0],4,n_blocks + 1,outputf);

        fclose(inf);
        remove(temp_name);
    }

    //free memory
    delete[] in_table;
    delete[] out_table;
    delete[] huffman.cann;
    delete[] huffman.nodes;
    delete[] huffman_pos.cann;
    delete[] huffman_pos.nodes;
}

/**************************************
Cyclic Redundancy Check (CRC)
***************************************/
void COMP_INFO::write_crc(bool wrt) {
    unsigned int bufLen;
    unsigned char buf[BLOCK_SIZE];
    unsigned long outCrc32 = 0,savedCrc32;

    pf = fopen(output_fname,"rb+");
    if(!pf) return;
    bufLen = (unsigned int) fread(buf,1,16,pf);
    outCrc32 = Crc32_ComputeBuf(outCrc32, buf,bufLen);
    fread(&savedCrc32,4,1,pf);
#ifdef BIGENDIAN
    bswap32(savedCrc32);
#endif
    fseek(pf,56,SEEK_SET);
    while(true) {
        bufLen = (unsigned int) fread(buf,1,BLOCK_SIZE,pf);
        if(bufLen == 0) break;
        outCrc32 = Crc32_ComputeBuf(outCrc32, buf, bufLen);
    }
    if(wrt) {
        fseek(pf,16,SEEK_SET);
        fwrite(&outCrc32,4,1,pf);
    }
    fclose(pf);

    if(!wrt) {
        if(savedCrc32 != outCrc32)
            printf("\"%s\" is Corrupted!!!\n",output_fname);
        else
            printf("\"%s\" is OK!\n",output_fname);
    }
}
/****************
*Debugging
*****************/
#ifdef _DEBUG
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
#endif
