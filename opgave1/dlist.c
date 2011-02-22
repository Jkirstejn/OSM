#ifndef _DLIST_C
#define _DLIST_C

#include <stdlib.h>
#include <stdio.h>

#include "bintree.h"
#include "dlist.h"
#include "main.h"

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
dlist_t* tree2dlist(tnode_t* tree) {
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
		// Free the allocated array of pointers to list elements
		free(container);
	}
	return dlist;
}

#endif
