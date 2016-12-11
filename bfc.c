#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int buf_size = 128;
int 

struct looptrack
{
	int loopno;
	struct looptrack* prev;
};

void help()
{
	printf(
	"\nbfc: a brainfuck compiler for 64 bit linux systems\n\n"

	"bfc <input file> [-o <output file>] [-v] [-h] [-c <compilation level=3>]\n\n"

	"PARAMETERS:\n\n"

	"\tinput file: This parameter is a path to the input file.\n\n"

	"OPTIONS:\n\n"

	"\t-o\tThis option allows the user to specify a path to an output file\n"
	"\t\tto which the final output shall be written.  The file need not\n"
	"\t\texist prior to the invocation of bfc.\n\n"

	"\t\tParameters:\n\n"

	"\t\toutput file: The path of the output file to be written.\n\n"

	"\t-v\tDirects bfc to print its version information prior to\n"
	"\t\tinitiating any compilation.\n\n"

	"\t-h\tDirects bfc to print usage help prior to initiating any\n"
	"\t\tcompilation.\n\n"

	"\t-c\tAllows the user to specify how far down the compilation chain\n"
	"\t\tbfc shall go.\n\n"

	"\t\tParameters:\n\n"

	"\t\tcompilation level: The point at which to stop compilation\n\n"

	"\t\t1 indicates to stop after producing assembly output.\n\n"

	"\t\t2 indicates to stop after producing an object file.\n\n"

	"\t\t3 indicates to compile down to executable form.\n\n"

	"\t\tIf the -c option is not added, a compilation level of\n"
	"\t\t3 will be used.\n"
	);
}

void version()
{
	printf("bfc version 0.1\n");
}

void parse_args(int argc, char** argv, int* isversion, int* ishelp, char** output, char** input, int* complev)
{
	int i;
	for(i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-v"))
		{
			*isversion = 1;
		}
		else if(!strcmp(argv[i], "-h"))
		{
			*ishelp = 1;
		}
		else if(!strcmp(argv[i], "-o"))
		{
			*output = argv[i+1];
			i++;
		}
		else if(!strcmp(argv[i], "-c"))
		{
			*complev = atoi(argv[i+1]);
			i++;
		}
		else
		{
			*input = argv[i];
		}
	}
}

void print_err()
{
	fprintf(stderr, "error\n");
}

int compile(FILE* in, FILE* out)
{
	fprintf(out, ".bss\n"
				 "OUTBUF:\n"
				 ".skip %d\n"
				 "DESTBUF:\n"
				 ".skip %d\n"
				 "INBUF:\n"
				 ".skip %d\n\n"
				 "STACK:\n"
				 ".skip %d\n\n"

				 ".data\n"
				 "ARR:\n"
				 ".space 32768\n\n"

				 ".text\n"
				 ".globl _flush_input\n"
				 ".type _flush_input, @function\n"

				 "_flush_input:"
				 "\tcmpq $DESTBUF, %%rsp\n"
				 "\tjne DO_INPUT_FLUSH\n"
				 "\tretq\n"
				 "\tDO_INPUT_FLUSH:\n"
				 "\tcmpq $OUTBUF, %%rbp\n"
				 "\tje NO_OUT_FLUSH:\n"
				 "\tcallq _flush_output\n"
				 "\tNO_OUT_FLUSH:\n"
				 "\tmovq $0, %%rax\n"
				 "\tmovq $0, %%rdi\n"
				 "\tmovq $INBUF, %%rsi\n"
				 "\tmovq %%rsp, %%rdx\n"
				 "\tsubq $DESTBUF, %%rdx\n"
				 "\tshr $4, %%rdx\n"
				 "\tsyscall\n"
				 "\tmovq $DESTBUF, %%r9\n"
				 "\tmovq $INBUF, %%r10\n"
				 "\tCOPY:\n"
				 "\tmovq (%%r9), %%r11\n"
				 "\tmovb (%%r10), %%bl\n"
				 "\tmovb %%bl, (%%r11)\n"
				 "\tcmpq %%r9, %%rsp\n"
				 "\tjne NEXT\n"
				 "\tretq\n"
				 "\tNEXT:\n"
				 "\taddq 4, %%r9\n"
				 "\tincq %%r10\n"
				 "\tjmp COPY\n"
				 ".globl _start\n"
				 ".type _start, @function\n"

				 "_start:\n"
				 "\tmovq $ARR, %%r8\n"
				 "\tmovq $OUTBUF, %%rbp\n"
				 "\tmovq $DESTBUF, %%rsp\n");
	int pincs = 0;
	int vincs = 0;
	int label = 0;
	int input_needs_flush = 0;
	struct looptrack* last = NULL;
	char c = fgetc(in);
	while(c != EOF)
	{
		if(c != '>' && c != '<' && pincs)
		{
			fprintf(out, "\t%s $%d, %%r8\n", pincs > 0 ? "addq" : "subq", abs(pincs));
			pincs = 0;
		}
		else if(c != '+' && c != '-' && vincs)
		{
			if(input_needs_flush)
			{
				fprintf(out, "\tcallq _flush_input\n");
			}
			fprintf(out, "\t%s $%d, (%%r8)\n", vincs > 0 ? "addb" : "subb", abs(vincs));
			vincs = 0;
		}
		if(c == '>')
		{
			pincs++;
		}
		else if(c == '<')
		{
			pincs--;
		}
		else if(c == '+')
		{
			vincs++;
		}
		else if(c == '-')
		{
			vincs--;
		}
		else if(c == '[')
		{
			fprintf(out, "\tjmp END%d\n"
						 "LABEL%d:\n", label, label);
			struct looptrack* tmp = malloc(sizeof(struct looptrack));
			tmp->loopno = label;
			tmp->prev = last;
			last = tmp;
			label++;
		}
		else if(c == ']')
		{
			if(last == NULL)
			{
				fprintf(stderr, "Error: Mismatched loop ending.\n");
				fclose(out);
				fclose(in);
				return 5;
			}
			else
			{
				fprintf(out, "END%d:\n"
							 "\tmovb (%%r8), %%cl\n"
							 "\ttestb %%cl, %%cl\n"
							 "\tjnz LABEL%d\n", last->loopno, last->loopno);
				struct looptrack* tmp = last;
				last = last->prev;
				free(tmp);
			}
		}
		else if(c == ',')
		{
			fprintf(out, "\tmovq $0, %%rax\n"
						 "\tmovq $0, %%rdi\n"
						 "\tmovq %%r8, %%rsi\n"
						 "\tmovq $1, %%rdx\n"
						 "\tsyscall\n");
		}
		else if(c == '.')
		{
			fprintf(out, "\tmovq $1, %%rax\n"
						 "\tmovq $1, %%rdi\n"
						 "\tmovq %%r8, %%rsi\n"
						 "\tmovq $1, %%rdx\n"
						 "\tsyscall\n");
		}

		c = fgetc(in);
	}
	if(last != NULL)
	{
		fprintf(stderr, "Error: mismatched loop beginning.\n");
		fclose(out);
		fclose(in);
		return 4;
	}
	fprintf(out, "\tmovq $60, %%rax\n"
				 "\tmovq $0, %%rdi\n"
				 "\tsyscall\n");
	fclose(in);
	fclose(out);
	return 0;
}

int assemble(char* in, char* out, int complev)
{
	char* tmp = calloc(strlen(out) + 10, sizeof(char));
	strcpy(tmp, out);
	strcpy(tmp+strlen(out), ".bfctmp.o");
	int pid = fork();
	if(pid)
	{
		int status;
		waitpid(pid, &status, 0);
		if(WIFEXITED(status) && WEXITSTATUS(status))
		{
			int exit = WEXITSTATUS(status);
			printf("Error: Compilation failed on assembly stage.\n");
			return 6;
		}
		else if(WIFSIGNALED(status))
		{
			printf("Error: Compilation interrupted on assembly stage\n");
			return 7;
		}
		int pid = fork();
		if(pid)
		{
			waitpid(pid, &status, 0);
			if(WIFEXITED(status) && WEXITSTATUS(status))
			{
				int exit = WEXITSTATUS(status);
				printf("Error: Compilation failed on linking stage.\n");
				remove(tmp);
				return 6;
			}
			else if(WIFSIGNALED(status))
			{
				printf("Error: Compilation interrupted on linking stage\n");
				remove(tmp);
				return 7;
			}
		}
		else
		{
			if(complev > 2)
			{
				FILE* ld = fopen("bfc.ld", "w");
				fprintf(ld, "ENTRY(_start)"
							"PHDRS\n"
							"{\n"
							"\theaders PT_PHDR PHDRS ;\n"
							"\ttext PT_LOAD FILEHDR PHDRS ;\n"
							"\tdata PT_LOAD ;\n"
							"\tbss PT_LOAD ;\n"
							"}\n"
							"SECTIONS\n"
							"{\n"
  							"\t. = 0x400000;\n"
  							"\t.bss . : { *(.bss) } :bss\n"
  							"\t.text . : { *(.text) } :text\n"
  							"\t. = ALIGN(4096);\n"
  							"\t.data . : { *(.data) } :data\n"
							"}");
				fclose(ld);
				char* ldargs[] = {"ld", tmp, "-T", "bfc.ld", "-o", out};
				execvp("ld", ldargs);
			}
		}
		return 0;
	}
	else
	{
		if(complev > 2)
		{
			char* asargs[] = {"as", in, "-o", tmp, 0};
			execvp("as", asargs);
		}
		else
		{
			char* asargs[] = {"as", in, "-o", out, 0};
			execvp("as", asargs);
		}
	}
}

int main(int argc, char** argv)
{
	int isversion = 0;
	int ishelp = 0;
	int complev = 3;
	char* output = "out";
	char* input = NULL;
	parse_args(argc, argv, &isversion, &ishelp, &output, &input, &complev);
	int purposeful = 0;
	if(ishelp)
	{
		help();
		purposeful = 1;
	}
	if(isversion)
	{
		version();
		purposeful = 1;
	}
	if(input != NULL)
	{
		FILE* in;
		FILE* out;
		if(!(in = fopen(input, "r")))
		{
			fprintf(stderr, "Fatal Error: Could not open input file:\n");
			print_err();
			return 2;
		}
		if(!(out = fopen(output, "w")))
		{
			fprintf(stderr, "Fatal Error: Could not open output file:\n");
			print_err();
			return 3;
		}
		int rescomp;
		if(rescomp = compile(in, out))
		{
			remove(output);
			return rescomp;
		}
		else if(complev > 1)
		{
			char* newout = calloc(strlen(output) + 10, sizeof(char));
			strcpy(newout, output);
			strcpy(newout+strlen(output), ".bfctmp.s");
			rename(output, newout);
			//in = fopen(newout, "r");
			//out = fopen(output, "w");
			int resas;
			if(resas = assemble(newout, output, complev))
			{
				remove(output);
				remove(newout);
				return resas;
			}
			else
			{
				remove(newout);
			}
		}
	}
	else if(!purposeful && output == NULL)
	{
		fprintf(stderr, "Fatal Error: No input file specified.\n");
		return 1;
	}
	return 0;
}