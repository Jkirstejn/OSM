#ifndef _MAIN_C
#define _MAIN_C

#include <stdlib.h>
#include <stdio.h>
#include "main.h"

// Function to compare two int pointers - for testing insert2
int compare(void* x, void* y) {
	int tx = *(int*)x;
	int ty = *(int*)y;
	int z = 1;
	if (tx < ty) {
		z = -1;
	} else if (tx == ty) {
		z = 0;
	}
	return z;
}

int main() {
	// Test of insert
	tnode_t* tree = NULL;
	insert(&tree, 4);
	insert(&tree, 5);
	insert(&tree, 8);
	insert(&tree, 2);
	insert(&tree, 6);
	insert(&tree, 7);
	printf("Test of inorder:\n");
	print_inorder(tree);

	// Test of size function
	int count = size(tree);
	printf("Test of size: %d\n", count);

	// Test of to_array
	int* toarray = to_array(tree);
	printf("Test of to_array:\n");
	for(int x = 0; count > x; x++) {
		printf("%d\n", toarray[x]);
	}

	// Test of tree2list
	dlist_t* dlist;
	dlist = tree2dlist(tree);
	// Go through list 2 times using the prev variable
	printf("Test of dlist using prev:\n");
	for(int x = 0; (count*2) > x; x++) {
		printf("\tData: %d\n", dlist->data);
		dlist = dlist->prev;
	}
	// Go through list 2 times using the next variable
	printf("Test of dlist using next:\n");
	for(int x = 0; (count*2) > x; x++) {
		printf("\tData: %d\n", dlist->data);
		dlist = dlist->next;
	}

	// Test of insert2
	tnode_t2* tree2 = NULL;
	int k = 5;
	int b = 9001;
    int t = 3;
	insert2(&tree2, &k, compare);
	insert2(&tree2, &b, compare);
	insert2(&tree2, &t, compare);
	printf("Test of insert2:\n\t%d\n", *((int*)tree2->data));
	printf("\t%d\n", k);

	printf("Test of insert2 in right child:\n\t%d\n", *((int*)tree2->rchild->data));
	printf("\t%d\n", b);

	printf("Test of insert2 in left child:\n\t%d\n", *((int*)tree2->lchild->data));
	printf("\t%d\n", t);
	return 0;
}
#endif
