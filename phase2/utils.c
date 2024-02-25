#include "./headers/utils.h"
#include "../headers/listx.h"

int contains(struct list_head *head, struct list_head *elem){
    struct list_head* iter;
        list_for_each(iter, head) {
            if(iter == elem)
                return 1;
        }
        return 0;
}

void *memcpy (void *dest, const void *src, unsigned int len)
{
  char *d = dest;
  const char *s = src;
  while (len--)
    *d++ = *s++;
  return dest;
}

void debug (void *arg) {}
