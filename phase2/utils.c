#include "./headers/utils.h"
#include "../headers/listx.h"

/**
 * Given a head and an element, this function will return true if the element is contained in the list
 * pointed by the head
*/
int contains(struct list_head *head, struct list_head *elem){
  struct list_head* iter;
  list_for_each(iter, head) {
    if(iter == elem)
      return 1;
  }
  return 0;
}

/**
 * memcpy is a standard C function that will copy memory blocks from one address to another
 * @param dest pointer to the address where memory will be copied
 * @param src pointer to the address from where memory will be copied
 * @param len number of the memory areas as char that must be copied
*/
void *memcpy (void *dest, const void *src, unsigned int len)
{
  char *d = dest;
  const char *s = src;
  while (len--)
    *d++ = *s++;
  return dest;
}

/**
 * Simple helper function to put a breakpoint on
 */
void debug (void *arg) {}
