#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "etc/phc-winner-argon2/include/argon2.h"

#define HASHLEN 33
#define SALTLEN 17
#define HASHSTORELEN 16
#define SALTSTORELEN 8
#define DIRLEN 1024
#define USERNAMELEN 16
#define NAMELEN 16
#define PASSWORDLEN 64

char* barray2hexstr (const unsigned char * data, size_t datalen);
unsigned char * hexstr2char(const char* hexstr);
int run(char *cmd, int input, int first, int last);
void split(char *cmd);
char* skipwhite(char *s);
int addUser();
void hash(char *password, unsigned char *salt, unsigned char *hashedPassword);
int login();
int shell (char *username);
int getUserCount();
void incrementUserCount();

char *args[512];
char line[1024];
int n = 0;
pid_t pid;
int command_pipe[2];

void hash(char *password, unsigned char *salt, unsigned char *hashedPassword) {
	unsigned char *pwd = (unsigned char *)strdup(password);
	uint32_t pwdlen = strlen((char *)pwd);
	uint32_t t_cost = 2;
	uint32_t m_cost = (1<<16);
	uint32_t parallelism = 1;
	argon2i_hash_raw(t_cost, m_cost, parallelism, pwd, pwdlen, salt, SALTLEN, hashedPassword, HASHLEN);
	free(pwd);
}

int getUserCount() {
	FILE *file;
	int count;
	if ((file = fopen("./etc/util", "r")) == NULL) {
		printf("Error: Unable to open ./etc/util");
		exit(1);
	} 
	fscanf(file, "%d", &count);
	fclose(file);
	return count;
}

void incrementUserCount() {
	FILE *file;
	int count = getUserCount() + 1;
	if ((file = fopen("./etc/util", "w")) == NULL) {
		printf("Error: Unable to open ./etc/util");
		_exit(1);
	} 
	fprintf(file, "%d", count);
	fclose(file);
}

int command(int input, int first, int last) {
	int pipes[2];
	pipe(pipes);
	pid = fork();
	if (pid == 0) {
	if (first == 1 && last == 0 && input == 0) {
			dup2(pipes[1], STDOUT_FILENO);
		} else if (first == 0 && last == 0 && input != 0) {
			dup2(input, STDIN_FILENO);
			dup2(pipes[1], STDOUT_FILENO);
		} else {
			dup2(input, STDIN_FILENO );
		}
		if (execvp( args[0], args) == -1) {
		_exit(EXIT_FAILURE);
		}
	}
	if (input != 0) {
		close(input);
	}
	close(pipes[1]);
	if (last == 1) {
		close(pipes[0]);
	}
	return pipes[0];
}

int run(char *cmd, int input, int first, int last) {
	cmd = skipwhite(cmd);
	char *next = strchr(cmd, ' ');
	int i = 0;
	while(next != NULL) {
		next[0] = '\0';
		args[i] = cmd;
		++i;
		cmd = skipwhite(next + 1);
		next = strchr(cmd, ' ');
	}
	if (cmd[0] != '\0') {
		args[i] = cmd;
		next = strchr(cmd, '\n');
		next[0] = '\0';
		++i; 
	}
	args[i] = NULL;
	if (args[0] != NULL) {
		if (strcmp(args[0], "exit") == 0) {
			exit(0);
		}
		if (strcmp(args[0], "cd") == 0) {
			chdir(args[1]);
			return 0;
		}
		if (strcmp(args[0], "add_user") == 0) {
			addUser();
			return 0;
		}
		n += 1;
		return command(input, first, last);
	}
	return 0;
}

char* skipwhite(char *s) {
	while (isspace(*s)) ++s;
	return s;
}

int shell (char *username) {
		while (1) {
		printf("%s@mysh> ", username);
		fflush(NULL);
		if (!fgets(line, 1024, stdin)) {
			return 0;
		}
		int input = 0;
		int first = 1;
		char *cmd = line;
		char *next = strchr(cmd, '|');
		while (next != NULL) {
			*next = '\0';
			input = run(cmd, input, first, 0);
			cmd = next + 1;
			next = strchr(cmd, '|');
			first = 0;
		}
		input = run(cmd, input, first, 1);
		for (int i = 0; i < n; ++i) {
			wait(NULL); 
		}
		n = 0;
	}
	return 0;
}

int login () {
	char username[USERNAMELEN];
	char password[PASSWORDLEN];
	char *name;
	char *home;
	printf("Login\n-----\n");
	printf("username: ");
	scanf("%s", &username);
	*password = *getpass("password: ");
	printf("\n");
	FILE *file;
	if ((file = fopen("./etc/passwd", "r")) == NULL) {
		printf("Error: Unable to open ./etc/passwd\n");
		_exit(EXIT_FAILURE);
	} 
	for (int i = 0; i < getUserCount(); i++) {
		char *tempUsername;
		char *storedPasswordHex;
		char *storedSaltHex;
		char buffer[1000];
		unsigned char hashedPassword[HASHLEN];
		fscanf(file, "%s\n", buffer);
		tempUsername = strtok(buffer, ";");
		storedPasswordHex = strtok(NULL, ";");
		storedSaltHex = strtok(NULL, ";");
		home = strtok(NULL, ";");
		name = strtok(NULL, ";");
		if (strcmp(username, tempUsername) == 0) {
		hash(password, hexstr2char(storedSaltHex), hashedPassword);
		if (strcmp(barray2hexstr(hashedPassword, HASHLEN), storedPasswordHex) == 0) {
				printf("Welcome %s!\n", name);
				fclose(file);
				chdir(home);
				shell(username);
				return 0;
		} else {
			printf("Invalid Password.\n");
			fclose(file);
			login();
		}
		}
	}
	printf("Username not found.\n");
	fclose(file);
	login();
}

int addUser() {
	char username[USERNAMELEN];
	char password[PASSWORDLEN];
	char name[NAMELEN];
	char home[DIRLEN];
	printf("Registering a new user:\nUsername: ");
	scanf("%s", &username);
	*password = *getpass("password: ");
	printf("Home Directory: ");
	scanf("%s", &home);
	printf("Name: ");
	scanf("%s", &name);
	FILE *file;
	unsigned char hashedPassword[HASHLEN];
	unsigned char salt[SALTLEN];
	int fd = open("/dev/urandom", O_RDONLY);
	read(fd, salt, SALTLEN);
	close(fd);
	hash(password, salt, hashedPassword);
	file = fopen("./etc/passwd", "a");
	fprintf(file, "%s;%s;%s;%s;%s\n", username, barray2hexstr(hashedPassword, HASHLEN), barray2hexstr(salt, SALTLEN), home, name);
	fclose(file);
	incrementUserCount();
	return 0;
}


char* barray2hexstr (const unsigned char * data, size_t datalen) {
	size_t final_len = datalen * 2;
	char* chrs = (unsigned char  *) malloc((final_len + 1) * sizeof(*chrs));
	unsigned int j = 0;
	for(j = 0; j<datalen; j++) {
		chrs[2*j] = (data[j]>>4)+48;
		chrs[2*j+1] = (data[j]&15)+48;
		if (chrs[2*j]>57) {
			chrs[2*j]+=7;
		}
		if(chrs[2*j+1]>57) {
			chrs[2*j+1]+=7;
		}
	}
	chrs[2*j]='\0';
	return chrs;
}

unsigned char *hexstr2char(const char* hexstr) {
	size_t len = strlen(hexstr);
	if(len % 2 != 0){
		return NULL;
		}
	size_t final_len = len / 2;
	unsigned char * chrs = (unsigned char *)malloc((final_len+1) * sizeof(*chrs));
	for (size_t i=0, j=0; j<final_len; i+=2, j++)
	        chrs[j] = (hexstr[i] % 32 + 9) % 25 * 16 + (hexstr[i+1] % 32 + 9) % 25;
	    chrs[final_len] = '\0';
	return chrs;
}

int main() {
	if (getUserCount() == 0) {
		addUser();
		return 0;
	}
	login();
	return 0;
}