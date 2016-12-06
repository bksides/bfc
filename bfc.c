#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>


struct looptrack
{
	int loopno;
	struct looptrack* prev;
};

void help()
{
	printf("No help menu yet.  Stay tuned :^)\n");
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
		if(!strcmp(argv[i], "-h"))
		{
			*ishelp = 1;
		}
		if(!strcmp(argv[i], "-o"))
		{
			*output = argv[i+1];
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
	fprintf(out, ".data\n"
				 "ARR:\n"
				 ".space 32430\n"
				 ".text\n"
				 ".globl _start\n"
				 ".type _start, @function\n"
				 "_start:\n"
				 "\tmovq $ARR, %%r8\n");
	int pincs = 0;
	int vincs = 0;
	int label = 0;
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

int assemble(char* in, char* out)
{
	int pid = fork();
	if(pid)
	{
		int status;
		waitpid(pid, &status, 0);
		if(WIFEXITED(status) && WEXITSTATUS(status))
		{
			int exit = WEXITSTATUS(status);
			printf("Error: Compilation failed on assembly stage.\n");
			//return 6;
			return 0;
		}
		else if(WIFSIGNALED(status))
		{
			printf("Error: Compilation interrupted on assembly stage\n");
			//return 7;
			return 0;
		}
		return 0;
	}
	else
	{
		/*
		char* tmp = calloc(strlen(out) + 10, sizeof(char));
		strcpy(tmp, out);
		strcpy(tmp+strlen(out), ".bfctmp.o");
		printf("%s\n", tmp);
		execle("/usr/bin/as", in, "-o", tmp);
		printf("Made it past assembly.\n");
		execle("/usr/bin/ld", "-nostdlib", tmp, "-o", out);
		*/
		char* args[] = {"gcc", "-nostdlib", in, "-o", out, 0};
		execvp("/usr/bin/gcc", args);
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
			if(resas = assemble(newout, output))
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
	else if(!purposeful || output != NULL)
	{
		fprintf(stderr, "Fatal Error: No input file specified.\n");
		return 1;
	}
	return 0;
}