#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
static uint8_t heap[8*1024*1024]; static uint32_t heap_pos=0;
void n64_mem_init(void){heap_pos=0;}
void *n64_mem_alloc(uint32_t s){s=(s+15)&~15;if(heap_pos+s>sizeof(heap))return 0;void*p=&heap[heap_pos];memset(p,0,s);heap_pos+=s;return p;}
#include "../src/esm/mw_esm.h"
int main(int argc,char**argv){
    FILE*f=fopen(argv[1],"rb");fseek(f,0,SEEK_END);long sz=ftell(f);rewind(f);
    uint8_t*d=malloc(sz);fread(d,1,sz,f);fclose(f);
    esm_reader_t r;esm_reader_init(&r,d,sz);
    while(esm_next_record(&r)){
        char n[5]={r.rec_name&0xFF,(r.rec_name>>8)&0xFF,(r.rec_name>>16)&0xFF,(r.rec_name>>24)&0xFF,0};
        printf("REC: %s  size=%u  pos=%u\n",n,r.rec_size,r.pos);
        while(esm_next_subrecord(&r)){
            char sn[5]={r.sub_name&0xFF,(r.sub_name>>8)&0xFF,(r.sub_name>>16)&0xFF,(r.sub_name>>24)&0xFF,0};
            printf("  SUB: %s  size=%u\n",sn,r.sub_size);
            esm_skip_subrecord(&r);
        }
        esm_skip_record(&r);
    }
    free(d);
}
