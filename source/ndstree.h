// SPDX-FileNotice: Modified from the original version by the BlocksDS project, starting from 2023.

#pragma once

inline int cmp(char *a, bool a_isdir, char *b, bool b_isdir)
{
	(void)a_isdir;
	(void)b_isdir;

	// oh... directory sort doesn't matter since we write out dir- and filenames seperately
	//if (a_isdir && !b_isdir) return -1;
	//if (b_isdir && !a_isdir) return +1;
	return strcmp(a, b);
}

struct TreeNode
{
	unsigned int dir_id;		// directory ID in case of directory entry
	char *fs_path;				// full path to the file or directory in the host PC
	char *name;					// file or directory name
	TreeNode *directory;		// nonzero indicates directory. first directory node is a dummy
	TreeNode *prev, *next;		// linked list

	TreeNode()
	{
		dir_id = 0;
		fs_path = (char *)"";
		name = (char *)"";
		directory = 0;
		prev = next = 0;
	}

	// new entry in same directory
	TreeNode *New(char *fs_path, char *name, bool isdir)
	{
		TreeNode *newNode = new TreeNode();
		newNode->fs_path = strdup(fs_path);
		newNode->name = strdup(name);

		TreeNode *node = this;

		if (cmp(name, isdir, node->name, node->dir_id) < 0)	// prev
		{
			while (cmp(name, isdir, node->name, node->dir_id) < 0)
			{
				if (node->prev)
					node = node->prev;
				else
					break;		// insert after dummy node
			}
		}
		else
		{
			while (node->next && (cmp(name, isdir, node->next->name, node->next->dir_id) >= 0))
			{
				node = node->next;
			}
		}

		// insert after current node
		newNode->prev = node;
		newNode->next = node->next;
		if (node->next) node->next->prev = newNode;
		node->next = newNode;

		return newNode;
	}

	// This looks for any entry called like the provided name inside the
	// directory this node belongs to.
	TreeNode *Find(const char *name)
	{
		TreeNode *node = this;

		// Start from the start of the linked list
		while (node->prev)
			node = node->prev;

		while (1)
		{
			if (strcmp(name, node->name) == 0)
				return node;

			node = node->next;
			if (node == NULL)
				return NULL;
		}
	}
};

TreeNode *ReadDirectory(TreeNode *node, char *path);
