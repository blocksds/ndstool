// SPDX-FileNotice: Modified from the original version by the BlocksDS project, starting from 2023.

#include "ndstool.h"
#include "ndstree.h"

/*
 * Variables
 */
unsigned int _entry_start;			// current position in name entry table
unsigned int file_top;				// current position to write new file to
unsigned int free_dir_id = 0xF000;	// incremented in ReadDirectory
unsigned int directory_count = 0;	// incremented in ReadDirectory
unsigned int file_count = 0;		// incremented in ReadDirectory
unsigned int total_name_size = 0;	// incremented in ReadDirectory
unsigned int file_end = 0;			// end of all file data. updated in AddFile
unsigned int free_file_id = 0;		// incremented in AddDirectory

/*
 * ReadDirectory
 * Read directory tree into memory structure
 * returns first (dummy) node
 */
TreeNode *ReadDirectory(TreeNode *node, char *path)
{
	TreeNode *first = node;

	// Start adding nodes from the end of the list
	while (node->next)
		node = node->next;

	//printf("%s\n", path);

	DIR *dir = opendir(path);
	if (!dir)
	{
		fprintf(stderr, "Cannot open directory '%s'.\n", path);
		exit(1);
	}

	struct dirent *de;
	while ((de = readdir(dir)))
	{
		// Exclude all directories starting with .
		if (!strncmp(de->d_name, ".", 1))
			continue;

		char strbuf[MAXPATHLEN];
		strcpy(strbuf, path);
		strcat(strbuf, "/");
		strcat(strbuf, de->d_name);
		//printf("%s\n", strbuf);

		struct stat st;
		if (stat(strbuf, &st))
		{
			fprintf(stderr, "Cannot get stat of '%s'.\n", strbuf);
			exit(1);
		}

		//if (S_ISDIR(st.st_mode) && !subdirs) continue;		// skip subdirectories

		total_name_size += strlen(de->d_name);

		if (S_ISDIR(st.st_mode))
		{
			// Check if this directory is already in one of the nodes. If it's
			// there, open the directory node and add new files. If not, create
			// a new directory node.
			TreeNode *found = first->Find(de->d_name);
			bool dir_found = false;
			if (found)
			{
				if (found->directory)
				{
					// There is a directory with the same name, we can combine
					// the files in both.
					dir_found = true;
				}
				else
				{
					// There is a file with the same name, so we can't create
					// this directory.
					fprintf(stderr, "Trying to create directory but a file with the same name already exists: %s\n", strbuf);
					exit(EXIT_FAILURE);
				}
			}

			if (dir_found)
			{
				// Add more files to the old list
				ReadDirectory(found->directory, strbuf);
			}
			else
			{
				node = node->New(strbuf, de->d_name, true);
				node->dir_id = free_dir_id++;
				directory_count++;
				node->directory = ReadDirectory(new TreeNode(), strbuf);
			}
		}
		else if (S_ISREG(st.st_mode))
		{
			// Check if there's already a file or directory with the same name
			TreeNode *found = first->Find(de->d_name);
			if (found)
			{
				fprintf(stderr, "Trying to create file but an entry with the same name already exists: %s\n", strbuf);
				exit(EXIT_FAILURE);
			}

			node = node->New(strbuf, de->d_name, false);
			file_count++;
		}
		else
		{
			fprintf(stderr, "'%s' is not a file or directory!\n", strbuf);
			exit(1);
		}
	}
	closedir(dir);

	while (node->prev)
		node = node->prev;	// return first
	return node;
}
