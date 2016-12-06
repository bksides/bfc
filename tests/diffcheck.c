#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char** argv)
{
	char* filename = argv[1];
	char* out = malloc(strlen(filename) + 3);
	strcpy(out, filename);
	strcpy(out+strlen(filename), ".o");
	char* check = malloc(strlen(filename) + 7);
	strcpy(check, filename);
	strcpy(check+strlen(filename), ".check");
	int pid = fork();
	if(pid)
	{
		int status;
		waitpid(pid, &status, 0);
		if(WIFEXITED(status))
		{
			if(WEXITSTATUS(status))
			{
				printf("\n%s: fail\n", filename);
				return 0;
			}
			printf("\n%s: pass\n", filename);
			return 0;
		}
		printf("\n%s: fail\n", filename);
		return 0;
	}
	else
	{
		char* args[] = {"diff", out, check, (char*)NULL};
		execvp("diff", args);
		return errno;
	}
}