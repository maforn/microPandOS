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

// TODO: controllare funzioni
void *memcpy(void *dst, const void *src, unsigned int len) {
	unsigned int i;
	if ((unsigned int)dst % sizeof(long) == 0 &&
             (unsigned int)src % sizeof(long) == 0 &&
             len % sizeof(long) == 0) {

                 long *d = dst;
                 const long *s = src;

                 for (i=0; i<len/sizeof(long); i++) {
                         d[i] = s[i];
                 }
         }
         else {
                 char *d = dst;
                 const char *s = src;

                 for (i=0; i<len; i++) {
                         d[i] = s[i];
                 }
         }

         return dst;
}