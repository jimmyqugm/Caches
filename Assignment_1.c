#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<stdbool.h>
#include<string.h>

//address bits layout
struct address{
	unsigned int address_bit;
	unsigned int offset_bit;
	unsigned int set_bit;
	unsigned int tag_bit;
};
//count hit or miss
struct data_match{
	unsigned int write_hit_count;
	unsigned int read_hit_count;
	unsigned int write_miss_count;
	unsigned int read_miss_count;
};
//block information
struct block{
	unsigned int LRU_level;
	unsigned int tag;
};
//cache information
struct cache{
	unsigned int size;
	unsigned int n_way;
	unsigned int n_set;
	unsigned int block_size;
	
	struct address address_info;
	struct block** block_info;
	struct data_match count;
};

//creat a empty cache
struct cache* creat_cache(int c_s, int way, int asso, int b_s, int n_tag, int index, int offset ){
	struct cache* new_cache = malloc(sizeof(struct cache));
	if (new_cache == NULL){
		printf("ERROR:Empty cache create fail\n");
		return NULL;
	}
	new_cache->block_info = (struct block**)malloc(asso * sizeof(struct block*));
	if (new_cache->block_info == NULL){
		printf("ERROR:set of block create fail\n");
		return NULL;
	}
	int i,j;
	for (i=0; i<asso; i++){
		new_cache->block_info[i] =(struct block*) malloc(way*sizeof(struct block));
		if (new_cache->block_info[i] == NULL){
		printf("ERROR:way of block create fail\n");
		return NULL;
	}

		for(j=0; j<way; j++){
			new_cache->block_info[i][j].LRU_level = way - 1;
			new_cache->block_info[i][j].tag = 0;
		}
	}
	new_cache->size= c_s;
	new_cache->n_way = way;
	new_cache->n_set = asso;
	new_cache->block_size = b_s;
	new_cache->count.write_hit_count = 0;
	new_cache->count.read_hit_count = 0;
	new_cache->count.write_miss_count = 0;
	new_cache->count.read_hit_count = 0;
	new_cache-> address_info.address_bit = 24;
	new_cache-> address_info.tag_bit = n_tag;
	new_cache-> address_info.set_bit = index;
	new_cache-> address_info.offset_bit = offset;
	return new_cache;
}

//free cache struct in memory
void free_cache(struct cache* new_cache){
	int i = 0;
	for(i=0; i<new_cache->n_set; i++){
		free(new_cache->block_info[i]);
	}
	free(new_cache->block_info);
	free(new_cache);
}

static FILE* reading_trace =NULL;

//read&write mode
enum R_TYPE{READ,WRITE};

//input reference  flie struct
struct reference{
	unsigned int r_address;
	enum R_TYPE type;
};

//open file
void open_file(char* path){
	reading_trace = fopen(path,"r");
	if (reading_trace == NULL){
		printf("ERROR:open file fail");
	}
}

//get reference from file
char reference_ad[200];
char type;
struct reference get_reference(void){
	struct reference reference_d;
	char reference_data[10]={0};
	fgets(reference_data,10, reading_trace);
	switch(reference_data[0]){
				case 'R':
					reference_d.type = READ;
					break;
				case 'W':
					reference_d.type = WRITE;
					break;
				default:
					break;
			}
	int i;
	//char reference_ad[8];
	for(i=0;i<200;i++){
		reference_ad[i] = reference_data[i+2];
	}
	sscanf(reference_ad, "%x", &reference_d.r_address);
	//printf("%x\n", reference_d.r_address);
	type = reference_data[0];
	return reference_d;
}

//extract tag from address
static unsigned int extract_tag(const unsigned int address, const struct cache* c){
	unsigned int shift;
	shift = c->address_info.set_bit + c->address_info.offset_bit;
	return address >> shift;
}

//extract index from address
static unsigned int extract_index(const unsigned int address, const struct cache* c){
	unsigned int mask = (1<< c->address_info.set_bit)-1;
	return (address >> c->address_info.offset_bit) & mask;
}

//extract offset from address
static unsigned int extract_offset(const unsigned int address, const struct cache* c){
	unsigned int mask = (1<< c->address_info.offset_bit)-1;
	return address & mask;
}


//figure out miss or not 
static bool miss(const unsigned int tag, const unsigned int index, const struct cache* c){
	struct block* c_r;
	c_r = c->block_info[index];
	unsigned int i = 0;
	bool miss = true;
	for(i=0; i<c->n_way; i++){
		struct block temp = c->block_info[index][i];
		if(temp.tag == tag){
			miss = false;
			break;
		}
	}
	return miss;
}

//LRU
int hit_way;
int LRU_uway;
static void LRU_miss(const unsigned int tag, const unsigned int index, const struct cache* c){
	unsigned int i = 0;
	unsigned int block_index = 0;
	for(i=0;i<c->n_way;i++){
		if(c->block_info[index][i].LRU_level ==  (c->n_way - 1)){
		c->block_info[index][i].tag = tag;
		c->block_info[index][i].LRU_level = 0;
		block_index = i;
		LRU_uway = i;
		break;
		}
	}
	for(i=0;i<c->n_way;i++){
		if(i!=block_index && c->block_info[index][i].LRU_level < (c->n_way-1))  c->block_info[index][i].LRU_level += 1;
	}
}
static void LRU_hit(const unsigned int tag, const unsigned int index, const struct cache* c){
	unsigned int i = 0;
	unsigned int block_index = 0;
	for(i=0;i<c->n_way;i++){
		if(c->block_info[index][i].tag == tag){
			block_index = i;
			hit_way = i;
			break;
		}
	}
	for(i=0;i<c->n_way;i++){
		if(c->block_info[index][block_index].LRU_level == 0) {
			LRU_uway = -1;
		}
		else {
			LRU_uway = -2;
			if(i==block_index) c->block_info[index][i].LRU_level = 0;
			if(i!=block_index && c->block_info[index][i].LRU_level < (c->n_way-1))  c->block_info[index][i].LRU_level += 1;
		}
	}
}


//compare cache & reference
int compare(const struct reference r, struct cache* c){
	unsigned int tag = extract_tag(r.r_address, c);
	unsigned int index = extract_index(r.r_address, c);
	if(miss( tag, index, c)){
		bool write = false;
		if(r.type == WRITE)
			write = true;
		LRU_miss(tag,index,c);
		switch(r.type){
			case READ:
				c->count.read_miss_count++;
				break;
			case WRITE:
				c->count.write_miss_count++;
				break;
		}
		return 0;	
	}else{
			LRU_hit(tag,index,c);
			switch(r.type){
			case READ:
				c->count.read_hit_count++;
				break;
			case WRITE:
				c->count.write_hit_count++;
				break;
		}	
		return 1;
	}
}

//Hex to Binary
void HextoB(char* Hex){
	int i = 0;
	while(Hex[i]){
		switch(Hex[i]){
			case '0':
			printf("0000");
			break;			
			case '1':
			printf("0001");
			break;			
			case '2':
			printf("0010");
			break;			
			case '3':
			printf("0011");
			break;			
			case '4':
			printf("0100");
			break;			
			case '5':
			printf("0101");
			break;			
			case '6':
			printf("0110");
			break;			
			case '7':
			printf("0111");
			break;			
			case '8':
			printf("1000");
			break;			
			case '9':
			printf("1001");
			break;			
			case 'a':
			printf("1010");
			break;			
			case 'b':
			printf("1011");
			break;			
			case 'c':
			printf("1100");
			break;			
			case 'd':
			printf("1101");
			break;			
			case 'e':
			printf("1110");
			break;			
			case 'f':
			printf("1111");
			break;
			default:
			break;	
		}
		i++;
	}
}

int main(int argc, char** argv){
	
	int b_s;
	int c_s;
	char w_p;
	int asso;
	//Check the input data
	if(argc != 9){
		printf("Please follow this input form: -b[blocksize] -c[cachesize] -w[writePolicy] -a[associtivity]\n");
		return -1;
	}
	//Input data to variable
	else{
		for(int i =1; i < argc; i ++){
			if(argv[i][0] == '-'){
				switch(argv[i][1]){
					case 'b':
						b_s = atoi(argv[i + 1]);
						break;
					case 'c':
						c_s = atoi(argv[i + 1]);
						break;
					case 'w':
						w_p = argv[i + 1][0];
						break;
					case 'a':
						asso = atoi(argv[i + 1]);
						break;
					default: 
						break;
				}
			}
		}	
	}
	int c_s_KB, way, b_n, tag, index, offset;
	float t_s, t_p;
	char* w_p_l;
	c_s_KB = c_s / 1024;
	way = (c_s / b_s) / asso;
	b_n = c_s / b_s;
	index = log(asso) / log(2);
	offset = log(b_s) / log(2);
	tag = 24 - index - offset;
	t_s = tag * b_n / 8;
	t_p = t_s / (t_s + c_s) * 100;
	if (w_p == 't'){
		w_p_l = "write-through";
	}else if (w_p == 'b'){
		w_p_l = "write-back";
	}
		
	printf("%dKB %d-way associative cache:\nBlock size = %d bytes\nNumber of [sets, blocks] = [%d,%d]\nExtra space for tag storage = %.0f bytes(%.2f%%)\nBits for [tag,index,offset]=[%d,%d,%d]= 24\nwrite plicy = %s\n", c_s_KB, way, b_s, asso, b_n, t_s, t_p, tag, index, offset, w_p_l);
	printf("Hex address\t Binary address\t\t Tag\t Set\t Blk\t Way\t UWay\t Read\t Write\n");
	printf("==================================================================================================================================\n");
	//create cache structure
	struct cache* cache = NULL;
	cache = creat_cache(c_s, way, asso, b_s, tag, index, offset);

	//input reference
	int tag_v;
	int set_v;
	int blk_v;
	int way_v;
	int uway_v;
	int read_v;
	int write_v;
	char* path = "/home/guangmin/part5_1.trc";
	int nread, nwrite =0;
	open_file(path);
	while(!feof(reading_trace)){
		struct reference reference_data;
		reference_data = get_reference();
		int r_hit = compare(reference_data, cache);
		tag_v = extract_tag(reference_data.r_address,cache);
		set_v = extract_index(reference_data.r_address,cache);
		blk_v = extract_offset(reference_data.r_address,cache);
		//way_v value update
		if(r_hit == 0)
			way_v = -1;
		else
			way_v = hit_way;
		//uway_v read_v write_v value update
		if(way_v != -1){
			read_v = 0;
			write_v = 0;
		if(type == 'R')
			nread ++;
		if(type == 'W')
			nwrite ++;
		}
		else if(type == 'R'){
			read_v = 1;
			write_v = 0;
			nread ++;
		}
		else if(type == 'W'){
			read_v = 0;
			write_v = 1;
			nwrite ++;
		}
		uway_v = LRU_uway;

		printf("\n%6x%c \t",reference_data.r_address,type);
		HextoB(reference_ad);
		printf("\t\t%d\t%5d\t%3d\t%4d\t%4d\t%4d\t%4d\n", tag_v,set_v,blk_v,way_v,uway_v,read_v,write_v);
	}
	int missed = cache->count.read_miss_count + cache->count.write_miss_count;
	int hits = cache->count.read_hit_count + cache->count.write_hit_count;
	float references = missed + hits;
	float hit_rate = hits/references;
	float miss_rate = missed/references;
	printf("nref= %d, nread = %d, nwrite = %d\n", missed + hits, nread, nwrite);	
	printf("hit: %d, hit_rate:%.2f\nmiss: %d, miss_rate:%.2f\n", hits, hit_rate, missed, miss_rate);
	free_cache(cache);
	return 0;
}
