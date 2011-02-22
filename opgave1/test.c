#ifndef _TREE_C
#define _TREE_C

#include <stdlib.h>
#include <stdio.h>

void insert(tnode_t** tnode, int data) {
	
	tnode_t* root;
	root = *tnode;
	
	// If current node is empty
	if (root == NULL) {
		// Allocate memory for new tnode
		root = malloc(sizeof(tnode_t));

		// Check if allocation went well
		if (root == 0) {
			out_of_memory();
		}

		root->data = data;
		root->lchild = NULL;
		root->rchild = NULL;
		*tnode = root;
	} else if (data <= root->data) {
		insert(&root->lchild, data);
	} else {
		insert(&root->rchild, data);
	}
}

// Inorder treewalk - prints out datavalues in ascending order
void print_inorder(tnode_t* tree) {
	// If left child is not empty, perform recursive call to left child
	if (tree->lchild != NULL) {
		print_inorder(tree->lchild);
	}
	// Print out data of current node
	printf("%d\n", tree->data);

	// If right child is not empty, perform recursive call to right child
	if (tree->rchild != NULL) {
		print_inorder(tree->rchild);
	}
}

int size(tnode_t* tree) {
	int count = 1;
	if (tree == NULL) {
		count = 0;
	} else {
		// If left child not empty; add size of the child-tree to count
		if (tree->lchild != NULL) {
			count = count+size(tree->lchild);
		}
		// If right child not empty; add size of the child-tree to count
		if (tree->rchild != NULL) {
			count= count+size(tree->rchild);
		}
	}
	return count;
}

// to_array help function for walking through the tree
void to_array_help(tnode_t* tree, int* pos, int* array) {
	if (tree->lchild != NULL) {
		to_array_help(tree->lchild, pos, array);
	}
	array[*pos] = tree->data;
	++*pos;
	if (tree->rchild != NULL) {
		to_array_help(tree->rchild, pos, array);
	}
}

// Converts a tree to an ordered array and returns a pointer to the first element of the array
int* to_array(tnode_t* tree) {
	int count = size(tree);
	int pos = 0;
	// Allocate space for data
	int* rtn = malloc(sizeof(int) * count);
	if (*rtn == 0) {
		out_of_memory();
	}
	to_array_help(tree, &pos, rtn);
	return rtn;
}

// Inserts a pointer to some data in a tree using the comparing function declared in the function call
// Using same structure as insert apart from the comparing of values
void insert2(tnode_t2** tnode, void* data, int (*comp)(void*, void*)) {
	tnode_t2* root;
	root = *tnode;
	if (root == NULL) {
		root = malloc(sizeof(tnode_t2));
		if (root == 0) {
			out_of_memory();
		}
		root->data = data;
		root->lchild = NULL;
		root->rchild = NULL;
		*tnode = root;
	} else if ((comp)(data, root->data) > 0) {
		insert2(&root->rchild, data, comp);
	} else {
		insert2(&root->lchild, data, comp);
	}
}

// Modulus function that wraps around (-1 = y)
int mod(int x, int y) {
	if (x < 0) {
		return mod(y+x, y);
	} else {
		int z = x / y;
		return (x - z * y);
	}
}

// Converts a tree to a circular double linked list
dlist_t* tree2list(tnode_t* tree) {
	dlist_t* dlist;
	dlist = NULL;
	dlist_t** container;
	int count = size(tree);
	if (count > 0) {
		// Allocate space for array of pointers to list elements
		container = malloc(sizeof(int) * count);
		if (container == 0) {
			out_of_memory();
		}
		int* data_array = to_array(tree);
		// Initialize and save all list elements with no next or prev
		for(int x = 0; count > x; x++) {
			dlist_t* current;
			current = malloc(sizeof(dlist_t));
			current->data = data_array[x];
			container[x] = current;
		}
		// Set up next and prev attributes for each list element
		for(int x = 0; count > x; x++) {
			container[x]->next = container[mod(x+1,count)];
			container[x]->prev = container[mod(x-1,count)];
		}
		dlist = container[0];
	}
	return dlist;
}
#endif
