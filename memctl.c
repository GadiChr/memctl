#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#define MEM_READ 0
#define MEM_WRITE 1

// memctl [read|write] [mainaddress] [offset]

void* mem;
struct Arguments{
	uint32_t address;
	uint32_t offset;
	uint32_t data;
	int mode;
	int regsize;
	int debug;
};

int init_mem(struct Arguments *args);
int read_mem(struct Arguments *args);
int write_mem(struct Arguments *args);
int proc_args(int argc, char *argv[], struct Arguments *args);
void print_help();
void print_args(struct Arguments *args);
void if_err_exit(int ret);

// ---- open mem device and map an address ----
int init_mem(struct Arguments *args){
	int fd;
	if((fd=open("/dev/mem", O_RDWR | O_SYNC))<0){
		printf("memctl_ERROR: cannot open \"/dev/mem\"\n");
		return -1;
	}
	mem = mmap(NULL, (args->offset)+1, PROT_READ | PROT_WRITE , MAP_SHARED, fd, args->address);
	if(args->debug)printf("--------mapping--------\n address mapped to 0x%08X\n",(int)mem);

	if((int)mem==0xffffffff){
		printf("memctl_ERROR: memory map failed\n");
		return -1;
	}
	if(close(fd)){	
		return -1;
	}
	return 0;
}

// ---- read from memory ----
int read_mem(struct Arguments *args){
	if(!(args->mode & 0b10))printf("|---address--|--offset--|--content---|\n");
	switch (args->regsize){
	case 32: printf("| 0x%08X |  0x%04X  | 0x%08X |\n", args->address, args->offset, *(((uint32_t*)mem)+args->offset/4)); break;
	case 16: printf("| 0x%08X |  0x%04X  |   0x%04X   |\n", args->address, args->offset, *(((uint16_t*)mem)+args->offset/2)); break;
	case  8: printf("| 0x%08X |  0x%04X  |    0x%02X    |\n", args->address, args->offset, *(((uint8_t*)mem)+args->offset)); break;
	default: printf("memctl_ERROR: wrong registersize\n"); return -1;
	};
	return 0;
}

// ---- write to memory ----
int write_mem(struct Arguments *args){
	printf("  write data to register ...\n");
	switch (args->regsize){
	case 32: *(((uint32_t*)mem)+args->offset/4) = args->data; break;
	case 16: *(((uint16_t*)mem)+args->offset/2) = (uint16_t)args->data; break;
	case  8: *(((uint8_t*)mem)+args->offset) = (uint8_t)args->data; break;
	default: return-1;
	}
	args->mode |= (1<<1);
	return 0;
}

// ---- handle given arguments ----
int proc_args(int argc, char *argv[], struct Arguments *args){
	if(argc<2){
		printf("memctl_ERROR: to few arguments\n");
		print_help();
		exit(0);
	}
	if(!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h")){	// HELP-MENU ?
		print_help();
		exit(0);
	}
	int i;
	char *endptr;
	for(i=1;i<argc;i++){ // iterate over all arguments
		if(!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")){	// DEBUGMODE ? 
			args->debug = 1;
			continue;
		}
		if(argv[i][1]=='='){	// check for optional args (e.g. w=VALUE or r=16)
			if(argv[i][0]=='w'){	// READ | WRITE ?
				args->mode = MEM_WRITE;
				args->data = (uint32_t)strtol(&(argv[i][2]), &endptr, 16);
			}
			else if(argv[i][0]=='r'){	// REGISTER-SIZE ?
				args->regsize = (uint32_t)strtol(&(argv[i][2]), &endptr, 10);
				if(args->regsize!=32 && args->regsize != 16 && args->regsize!= 8){
					printf("memctl_ERROR: register-size limitted to 8, 16 and 32 Bit (default = 32 Bit)\n");
					return -1;
				}
			}
			if(*endptr!='\0'){	// indicates if an invalid char was found inside an argument (see strtol)
				printf("memctl_ERROR: invalid argument: %s\n", argv[i]);
				print_help();
				return -1;
			}
			continue;
		}
		// only 2 arguments are not prefixed by a character, those are address and offset
		if(args->address==0){	// ADDRESS ?
			args->address = (uint32_t)strtol(argv[i], &endptr, 16);
		}
		else{	// OFFSET ?
			args->offset = (uint32_t)strtol(argv[i], &endptr, 16);
		}
		if(*endptr!='\0'){ // indicates if an invalid char was found inside an argument (see strtol)
			printf("memctl_ERROR: invalid argument: %s\n", argv[i]);
			print_help();	
			return -1;
		}
	}
	if(args->address==0){ // if no address was given 
		printf("memctl_ERROR: no address specified\n");
		return -1;
	}
	return 0;
}

// ---- print help lines ----
void print_help(){
	printf(" USAGE: memctl [ADDRESS] [OFFSET] [OPTIONS]\n");
	printf(" OPTIONS:\n");
	printf("      w=[HEX_DATA]  ... hexadecimal data to write into ADDRESS\n");
	printf("      r=[REG_SIZE]  ... size of the register in Bit\n");
	
}

// ---- print contents of struct Arguments args ----
void print_args(struct Arguments *args){
	printf("-------arguments-------\n");
	printf(" address = 0x%08X\n", args->address);
	printf(" offset  = 0x%08X\n", args->offset);
	printf(" data    = 0x%08X\n", args->data);
	printf(" mode    = %s\n", (args->mode==MEM_WRITE) ? "write" : "read");
	printf(" regsize = %d\n", args->regsize);
}

// ---- handle errors ----
void if_err_exit(int ret){
	if(ret==-1){
		printf("memctl_ERROR %d: exit program ...\n", ret);
		exit(ret); 
	}
}

// ----main function ----
int main(int argc, char *argv[]){
	int ret=0;	
	struct Arguments args = {0,0,0,MEM_READ,32,0};	// set default arguments
	ret = proc_args(argc, argv, &args);	
	if(args.debug)print_args(&args);
	if_err_exit(ret);
	
	if_err_exit(ret = init_mem(&args));	
	
	if_err_exit(read_mem(&args));	
	if(args.mode==MEM_WRITE){
		if_err_exit(write_mem(&args));	
		if_err_exit(read_mem(&args));	
	}
	
	if_err_exit(ret);
return 0;
}
