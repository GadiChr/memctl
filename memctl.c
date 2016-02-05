#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#define MEM_READ 0
#define MEM_WRITE 1

// memctl [read|write] [mainaddress] [offset]

void* mem;				// the pointer to which the memory will be mapped
struct Arguments{		// holds all necessary information about the process
	uint32_t address;	// the startaddress
	uint32_t offset;	// offset from the startaddress in pointersize-units
	uint32_t data;		// the data to write
	int mode;			// Bit[0] indicate writemode(1) or readmode(0), Bit[1] is used in the read_mem function 
	int regsize;		// holds the size of the target-register in Bit-units
	int debug;			// Bit[0] indicates verose(1) else (0)
	int simpleoutput;	// Bit[0] indicates simple-output(1) else (0)
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
		printf(" memctl_ERROR: cannot open \"/dev/mem\"\n");
		return -1;
	}
	mem = mmap(NULL, (args->offset)+1, PROT_READ | PROT_WRITE , MAP_SHARED, fd, args->address);
	if(args->debug)printf(" --------mapping--------\n address mapped to 0x%08X\n",(int)mem);

	if((int)mem==0xffffffff){
		printf(" memctl_ERROR: memory map failed\n");
		return -1;
	}
	if(close(fd)){	
		return -1;
	}
	return 0;
}

// ---- read from memory ----
int read_mem(struct Arguments *args){
	if(args->simpleoutput && (args->mode == 0 || args->mode == 0b11)){
		switch (args->regsize){
		case 32: printf("%08X\n", *(((uint32_t*)mem)+args->offset/4)); break;
		case 16: printf("%04X\n", *(((uint16_t*)mem)+args->offset/2)); break;
		case  8: printf("%02X\n", *(((uint8_t*)mem)+args->offset)); break;
		default: return-1;
		}
		return 0;
	}
	else if(!args->simpleoutput){
		if(!(args->mode & 0b10)){
			printf(" |---address--|--offset--|--content---|\n");
		}
		switch (args->regsize){
		case 32: printf(" | 0x%08X |  0x%04X  | 0x%08X |\n", args->address, args->offset, *(((uint32_t*)mem)+args->offset/4)); break;
		case 16: printf(" | 0x%08X |  0x%04X  |   0x%04X   |\n", args->address, args->offset, *(((uint16_t*)mem)+args->offset/2)); break;
		case  8: printf(" | 0x%08X |  0x%04X  |    0x%02X    |\n", args->address, args->offset, *(((uint8_t*)mem)+args->offset)); break;
		default: printf(" memctl_ERROR: wrong registersize\n"); return -1;
		};
	}
	return 0;
}

// ---- write to memory ----
int write_mem(struct Arguments *args){
	if(!args->simpleoutput)printf("  write data to register ...\n");
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
	if(argc<2){ // error if to FEW ARGUMENTS are given
		printf(" memctl_ERROR: to few arguments given\n");
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
		if(!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")){	// VERBOSE OUTPUT ? 
			args->debug = 1;
			continue;
		}
		if(!strcmp(argv[i], "-s") || !strcmp(argv[i], "--simple")){	// SIMPLE OUTPUT ? 
			args->simpleoutput = 1;
			continue;
		}
		if(argv[i][1]=='='){	// check for optional args (e.g. w=VALUE or r=16)
			if(argv[i][2]=='\0'){	// if no character follows the equal-sign
				printf(" memctl_ERROR: no argument found after %s\n", argv[i]);
				return -1;
			}
			if(argv[i][0]=='w' || argv[i][0]=='W'){	// READ | WRITE ?
				args->mode = MEM_WRITE;
				args->data = (uint32_t)strtoll(&(argv[i][2]), &endptr, 16);
			}
			else if(argv[i][0]=='r' || argv[i][0]=='R'){	// REGISTER-SIZE ?
				args->regsize = (uint32_t)strtoll(&(argv[i][2]), &endptr, 10);
				if(args->regsize!=32 && args->regsize != 16 && args->regsize!= 8){
					printf(" memctl_ERROR: register-size limitted to 8, 16 and 32 Bit (default = 32 Bit)\n");
					return -1;
				}
			}
			if(*endptr!='\0'){	// indicates if an invalid char was found inside an argument (see strtol)
				printf(" memctl_ERROR: invalid argument: %s\n", argv[i]);
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
			printf(" memctl_ERROR: invalid argument: %s\n", argv[i]);
			print_help();	
			return -1;
		}
	}
	if(args->address==0){ // if no address was given 
		printf(" memctl_ERROR: no address specified\n");
		return -1;
	}
	return 0;
}

// ---- print help lines ----
void print_help(){
	printf(
		"\n"
		" memctl is a commandline tool which allows you to read or write directly \n"
		" to the beaglebone's registers. It uses the mmap system call to map the \n"
		" register you want to use.\n"
		"\n"
		" USAGE: memctl {ADDRESS} [OFFSET] [OPTIONS]\n"
		"      arguments in curly brackets are necessary, but those in square\n"
		"      brackets are optional.\n"
		"\n"
		" OPTIONS:\n"
		"      w=[HEX_DATA]  ... hexadecimal data to write\n"
		"      r=[REG_SIZE]  ... size of the register in Bit\n"
		"\n"
	);
}

// ---- print contents of struct Arguments args ----
void print_args(struct Arguments *args){
	printf(" -------arguments-------\n");
	printf("  address = 0x%08X\n", args->address);
	printf("  offset  = 0x%08X\n", args->offset);
	printf("  data    = 0x%08X\n", args->data);
	printf("  mode    = %s\n", (args->mode==MEM_WRITE) ? "write" : "read");
	printf("  regsize = %d\n", args->regsize);
}

// ---- handle errors ----
void if_err_exit(int ret){
	if(ret==-1){
		printf(" memctl_ERROR: [%d] exit program ...\n", ret);
		exit(ret); 
	}
}

// ----main function ----
int main(int argc, char *argv[]){
	int ret=0;	
	struct Arguments args = {0,0,0,MEM_READ,32,0,0};	// set default arguments
	ret = proc_args(argc, argv, &args);	
	if(args.debug){
		print_args(&args);
	}
	if_err_exit(ret);
	
	if_err_exit(init_mem(&args));	
	
	if_err_exit(read_mem(&args));	
	
	if(args.mode==MEM_WRITE){
		if_err_exit(write_mem(&args));	
		if_err_exit(read_mem(&args));	
	}
	
	if_err_exit(ret);
	
	return 0;
}
