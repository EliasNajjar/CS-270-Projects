// Elias Najjar

#include "strmap.h"
#include <stdlib.h>

// hash a sequence of chars into an int
int hash(char *key, int size) {
    const int multi = 3;
    unsigned long result = 1000000007;

    for (int i = 0; key[i] != 0; ++i) {
        result = result * multi ^ key[i];
    }
    return result % size;
}

/* Create a new hashtab, initialized to empty. */
strmap_t *strmap_create(int numbuckets) {
    if (numbuckets < MIN_BUCKETS) {
        numbuckets = MIN_BUCKETS; // numbuckets cannot be less than MIN_BUCKETS
    }
    else if (numbuckets > MAX_BUCKETS) {
        numbuckets = MAX_BUCKETS; // or above MAX_BUCKETS
    }

    strmap_t *temp = (strmap_t *) malloc(sizeof(strmap_t)); // initialize map
    temp->strmap_buckets = (smel_t **)calloc(numbuckets,sizeof(smel_t*)); // initialize buckets
    temp->strmap_size = 0;
    temp->strmap_nbuckets = numbuckets;

    return temp;
}

/* Insert an element with the given key and value.
 *  Return the previous value associated with that key, or null if none.
 */
void *strmap_put(strmap_t *m, char *key, void *value) {
    int hash_value = hash(key, m->strmap_nbuckets);
    smel_t *bucket = m->strmap_buckets[hash_value]; // bucket to put item into
    smel_t *to_add = malloc(sizeof(smel_t));
    to_add->sme_key = key;
    to_add->sme_value = value;
    to_add->sme_next = NULL;

    if (bucket == NULL || strcmp(key, bucket->sme_key) < 0) {
        ++m->strmap_size;
        to_add->sme_next = bucket; // item should be first in list, point to old first
        m->strmap_buckets[hash_value] = to_add; // make bucket point to new item
        return NULL;
    }

    while (bucket->sme_next != NULL && strcmp(key, bucket->sme_next->sme_key) >= 0) {
        bucket = bucket->sme_next; // traverse until we have the place to put the item
    }

    if (strcmp(key, bucket->sme_key) > 0) {
        ++m->strmap_size;
        to_add->sme_next = bucket->sme_next;
        bucket->sme_next = to_add; // end: make bucket point to new item
        return NULL;
    }

    void *temp = bucket->sme_value;
    bucket->sme_value = value;
    return temp; // replace value and return old value
}

/* return the value associated with the given key, or null if none */
void *strmap_get(strmap_t *m, char *key) {
    smel_t *bucket = m->strmap_buckets[hash(key, m->strmap_nbuckets)]; // bucket the item would be in

    int diff;
    while (bucket != NULL && (diff = strcmp(key, bucket->sme_key)) > 0) {
        bucket = bucket->sme_next; // traverse until end or bucket key comes after or equals key
    }

    if (bucket == NULL || diff < 0) {
        return NULL; // if at end or bucket key comes after key, key is not there
    }
    return bucket->sme_value; // return value at key
}

/* remove the element with the given key and return its value.
   Return null if the hashtab contains no element with the given key */
void *strmap_remove(strmap_t *m, char *key) {
    int hash_value = hash(key, m->strmap_nbuckets);
    smel_t *bucket = m->strmap_buckets[hash_value]; // bucket the item would be in

    if (bucket == NULL) {
        return NULL; // no elements in bucket: key is not there
    }

    if (strcmp(key, bucket->sme_key) == 0) {
        --m->strmap_size;
        void *temp = bucket->sme_value;
        m->strmap_buckets[hash_value] = bucket->sme_next; // if key is first, make bucket point to next
        return temp;
    }

    int diff;
    while (bucket->sme_next != NULL && (diff = strcmp(key, bucket->sme_next->sme_key)) > 0) {
        bucket = bucket->sme_next; // traverse until end or bucket key comes after or equals key
    }

    if (bucket->sme_next == NULL || diff < 0) {
        return NULL; // if at end or bucket key comes after key, key is not there
    }

    --m->strmap_size;
    void *temp = bucket->sme_next->sme_value;
    bucket->sme_next = bucket->sme_next->sme_next; // set pointer before to pointer after
    return temp; // return value of removed key
}

/* return the # of elements in the hashtab */
int strmap_getsize(strmap_t *m) {
    return m->strmap_size;
}

/* return the # of buckets in the hashtab */
int strmap_getnbuckets(strmap_t *m) {
    return m->strmap_nbuckets;
}

/* print out the contents of each bucket */
void strmap_dump(strmap_t *m) {
    printf("total elements = %d\n", m->strmap_size);
    for (int i = 0; i < m->strmap_nbuckets; ++i) {
        smel_t *traverse = m->strmap_buckets[i];
        if (traverse != NULL) {
            printf("bucket %d:\n", i);
            while (traverse != NULL) {
                printf("\t%s->%p\n", traverse->sme_key, traverse->sme_value);
                traverse = traverse->sme_next;
            }
        }
    }
}
