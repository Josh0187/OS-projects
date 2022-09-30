#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <stdint.h>
#include <time.h>


int parseArgs(char * args[], int argc, int * l, int * r, int * i, char ** fileList) {
	// no args
	if (argc == 1) {
		return 0;
	}

	int list_count = 0;

	// read options first
	for (int k = 1; k < argc; k++ ) {
		// if option
		if (args[k][0] == '-') {
			for (int j = 1; j < 4; j++) {
				if (args[k][j] == 'l') {
					*l = 1;
				}
				
				else if (args[k][j] == 'R') {
					*r = 1;
				}
				else if (args[k][j] == 'i') {
					*i = 1;
				}
				else if (args[k][j] != '\n' && args[k][j] != 0 ) {
		
					printf("Error : Unsupported Option\n");
					return -1;
				}
				else {
					if (j == 1) {
						printf("Error : Unsupported Option\n");
						return -1;
					}
					else {
						break;
					}
				}
			}
		}
		
		else {
			fileList[list_count++] = strdup(args[k]);
		} 
	}
	
	return list_count;
}

// get file permission string
char* get_permissions(char *file, struct stat st){
    	char *modeval = malloc(sizeof(char) * 9 + 1);
    	mode_t perm = st.st_mode;
     	modeval[0] = (perm & S_IRUSR) ? 'r' : '-';
        modeval[1] = (perm & S_IWUSR) ? 'w' : '-';
        modeval[2] = (perm & S_IXUSR) ? 'x' : '-';
        modeval[3] = (perm & S_IRGRP) ? 'r' : '-';
        modeval[4] = (perm & S_IWGRP) ? 'w' : '-';
        modeval[5] = (perm & S_IXGRP) ? 'x' : '-';
        modeval[6] = (perm & S_IROTH) ? 'r' : '-';
        modeval[7] = (perm & S_IWOTH) ? 'w' : '-';
        modeval[8] = (perm & S_IXOTH) ? 'x' : '-';
        modeval[9] = '\0';
        return modeval;     
}

// compare function for qsort
int compareStrings(const void *a, const void *b) {
	const char **str_a = (const char **)a;
	const char **str_b = (const char **)b;
	return -strcmp(*str_a, *str_b);
}

int main(int argc, char * argv[]) {
	char ** file_list = malloc(20 * sizeof(char*));
	
	int i = 0;
	int l = 0;
	int r = 0;
	
	FILE * f;
	
	struct stat sb;
	struct passwd * pw;
	struct group *gr;
	struct tm * p;
	
	char * time = malloc(20 * sizeof(char));
	char * file_perms;
	char * abs_path;
	
	// alphasort
	int n;
	struct dirent ** dir_names;
	
	int isFilename;
	
	int count = parseArgs(argv, argc, &l, &r, &i, file_list);
	
	if (count < 0) {
		// unsupported option so exit
		return 0;
	}
	
	// no file path given
	if (count == 0) {
		file_list[0] = ".";
		count = 1;
	}

	int file_count = 0;
	while (file_count < count) {
		n = scandir(file_list[file_count], &dir_names, NULL, alphasort);
		
		char * dirName;
		if (n < 0) {
			if (file_list[file_count] != NULL) {
				// test if file can be opened
				f = fopen(file_list[file_count], "r");
				if (f != NULL) {
					fclose(f);
					dirName = strdup(file_list[file_count]);
					isFilename = 1;
				}
				else {
					printf("Error : Nonexistent files or directories\n");
					return 0;
				}
			}
		}
		
		int dir_counter = 0;	

		while (dir_counter < n || isFilename == 1) {
			if (isFilename != 1) {
				dirName = strdup(dir_names[dir_counter]->d_name);
				free(dir_names[dir_counter]);
			}
			
			if (strcmp(dirName , ".") != 0 && strcmp(dirName, "..") != 0 && (dirName[0]  != '.')) {
				// get file stats
				if (n < 0) {
					lstat(dirName, &sb);	
				}
				else {
					abs_path = strdup(file_list[file_count]);
					strcat(abs_path, "/");
					strcat(abs_path, dirName);
					stat(abs_path, &sb);
					free(abs_path);
				}
					
				// if i option specified, print ino
				if (i == 1) {
					printf("%ju ", (uintmax_t)sb.st_ino);
				}
						
				// if l option specified, print file permission, # of links, owner name, owner group, file size, last modified date
				if (l == 1) {
					// permissions
					file_perms = get_permissions(dirName, sb);
					printf("%s ", file_perms);
					free(file_perms);
					// link count
					printf("%2ju ", sb.st_nlink);
					// owner name
					pw =  getpwuid(sb.st_uid);
					printf("%s ", pw->pw_name);
					// group name
					gr = getgrgid(sb.st_gid);
					printf("%s ", gr->gr_name);
					// file size
					printf("%6jd ", (intmax_t) sb.st_size);
					// last modified date
					p = localtime(&sb.st_mtim.tv_sec);
					strftime(time, 20, "%b %e %Y %R", p);
					printf("%s ", time);		
				}
						
				// if long format used, use new line for each directory
				if (l == 1) {
					printf("%s\n", dirName);
				}
				// otherwise separated by spaces
				else {
					printf("%s  ", dirName);
				}
					
			}
			dir_counter++;
			isFilename = 0;
		}

		file_count++;
	}
	if (l != 1) {
		printf("\n");
	}
	free(file_list);
	if (n > 0) {
		free(dir_names);
	}
	free(time);
	return 0;
}


