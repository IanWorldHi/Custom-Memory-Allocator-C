#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "contiguous.h"

struct contiguous {
  struct cnode *first;
  void *upper_limit;
};

struct cnode {
  size_t nsize;
  struct cnode *prev;
  struct cnode *next;
  struct contiguous *block;
};

const int SIZEOF_CONTIGUOUS = sizeof(struct contiguous);
const int SIZEOF_CNODE = sizeof(struct cnode);

static const char STAR_STR[] = "*";
static const char NULL_STR[] = "NULL";

// maybe_null(void *p) return a pointer to "NULL" or "*",
//   indicating if p is NULL or not.
static const char *maybe_null(void *p) {
  return p ? STAR_STR : NULL_STR;
}

// gapsize(n0, n1) determine the size (in bytes) of the gap between n0 and n1.
static size_t gapsize(struct cnode *n0, struct cnode *n1) {
  assert(n0);
  assert(n1);
  void *v0 = n0;
  void *v1 = n1;
  return (v1 - v0) - n0->nsize - sizeof(struct cnode);
}

// print_gapsize(n0, n1) print the size of the gap between n0 and n1,
//     if it's non-zero.
 static void print_gapsize(struct cnode *n0, struct cnode *n1) {
  assert(n0);
  assert(n1);
  size_t gap = gapsize(n0, n1);

  if (gap != 0) {
    printf("%zd byte gap\n", gap);
  }
}
// pretty_print_block(chs, size) Print size bytes, starting at chs,
//    in a human-readable format: printable characters other than backslash
//    are printed directly; other characters are escaped as \xXX
static void pretty_print_block(unsigned char *chs, int size) {
  assert(chs);
  for (int i = 0; i < size; i++) {
    printf(0x20 <= chs[i] && chs[i] < 0x80 && chs[i] != '\\'
           ? "%c" : "\\x%02X", chs[i]);
  }
  printf("\n");
}

// print_node(node) Print the contents of node and all nodes that
//    follow it.  Return a pointer to the last node.
static struct cnode *print_node(struct cnode *node) {
  while (node != NULL) {
    void *raw = node + 1;     // point at raw data that follows.
    printf("struct cnode\n");
    printf("    nsize: %ld\n", node->nsize);
    printf("    prev: %s\n", maybe_null(node->prev));
    printf("    next: %s\n",  maybe_null(node->next));
    printf("%zd byte chunk: ", node->nsize);
    pretty_print_block(raw, node->nsize);

    if (node->next == NULL) {
      return node;
    } else {
      print_gapsize(node, node->next);
      node = node->next;
    }
  }
  return NULL;
}

static void print_hr(void) {
    printf("----------------------------------------------------------------\n");
}

// print_debug(block) print a long message showing the content of block.
void print_debug(struct contiguous *block) {
  assert(block);
  void *raw = block;
  print_hr();
  printf("struct contiguous\n");
  printf("    first: %s\n", maybe_null(block->first));
  if (block->first == NULL) {
    size_t gap = block->upper_limit - raw - sizeof(struct contiguous);
    printf("%zd byte gap\n", gap);
  } else {
    void *block_first = block->first;
    size_t gap = block_first - raw - sizeof(struct contiguous);
    if (gap) {
      printf("%zd byte gap\n", gap);
    }
  }
  struct cnode *lastnode = print_node(block->first);
  if (lastnode != NULL) {
    print_gapsize(lastnode, block->upper_limit);
  }
  print_hr();
}
// make_contiguous(size) Create a block including a buffer of size.
// This function does call malloc.
// effects: allocates memory.  Caller must call destroy_contiguous.
struct contiguous *make_contiguous(size_t size) {
  assert(size > 16);
  struct contiguous *block = malloc(sizeof(struct contiguous)+size);
  if (!block) {
    return NULL;
  }
  block->first = NULL;
  block->upper_limit = (char *)block + size+16;
  for(char* i = (char *)block+16; i< ((char *)block + size+16); i++) {
    *i = '$';
  }
  return block;
}

//destroy_contiguous(block) Cleans up block.
// effects: calls free.
void destroy_contiguous(struct contiguous *block) {
  if(block->first != NULL){
    printf("Destroying non-empty block!\n");
  }
  free(block);
}
// cfree(p) Remove the node for which p points to its data.
// Note: this function must not call free.
void cfree(void *p) {
  char* bob = p;
  for(int i = 0; i < ((struct cnode *)(bob-32))->nsize; i++) {
    *(bob+i) = '$';
  }
  if(((struct cnode *)(bob-32))->block->first == ((struct cnode *)(bob-32))) {
    ((struct cnode *)(bob-32))->block->first = ((struct cnode *)(bob-32))->next;
  }
  else if(((struct cnode *)(bob-32))->prev != NULL) {
    ((struct cnode *)(bob-32))->prev->next = ((struct cnode *)(bob-32))->next;
  }
  if(((struct cnode *)(bob-32))->next != NULL) {
    ((struct cnode *)(bob-32))->next->prev = ((struct cnode *)(bob-32))->prev;
  }
}

// cmalloc(block, size) Inside block, make a region of size bytes, and
// return a pointer to it.  If there is not enough space,
// return NULL.
// Note: this function must not call malloc.
void *cmalloc(struct contiguous *block, int size) {
  char count = 0;
  char* thingy = 0;
  for(char* i = (char *)block+16; i< (char *)block->upper_limit; i++) {
    if(count==size+32) {
      thingy= i - size;
      assert(thingy);
    }
    if(*i=='$') {
      count++;
    }
    else{
      count=0;
    }
  }
  if(thingy==0) {
    return NULL;
  }
  if ((thingy + size + sizeof(struct cnode)) > (char *)block->upper_limit) {
    return NULL;
  }
  else{
    struct cnode *node = (struct cnode *)thingy;
    assert(node);
    if (!node) {
      return NULL;
    }
    node->nsize = size;
    if(block->first == NULL) {
      block->first = node;
      node->prev = NULL;
      node->next = NULL;
    }
    else if (block->first != NULL && (char *)block->first>thingy) {
      node->prev = NULL;
      node->next = block->first;
      block->first->prev = node;
      block->first = node;
    }
    else if (block->first != NULL && (char *)block->first<thingy) {
      node->prev = block->first;
    }
    struct cnode* nexter = block->first;
    while((char *)nexter<thingy && nexter->next!=NULL) {
      nexter = nexter->next;
    }

 if(nexter==NULL){
      node->prev = NULL;
      node->next = NULL;
    }
    else if (nexter->next==NULL && (char *)nexter<thingy) {
      node->next = NULL;
      node->prev = nexter;
      nexter->next = node;
    }
    else if(nexter->prev==NULL) {
      node->next = nexter;
      node->prev = NULL;
      nexter->prev = node;
    }
    else if(nexter->prev!=NULL) {
      node->next = nexter;
      node->prev = nexter->prev;
      nexter->prev = node;
      node->prev->next = node;
    }
    node->block = block;
    for (int i = 0; i < size; i++) {
      *((char *)thingy + i) = '9';
    }
    return thingy + sizeof(struct cnode);
  }
}


