#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sum_array_direct (int *xs, int length) {
     int sum = 0;
     for (int i = 0; i < length; i++)
          sum += xs[i];
     return sum;
}

typedef int (*int_binop)(int, int);

int fold_int_array (int *array, int length, int init, int_binop op) {
     int value = init;
     for (int i = 0; i < length; i++) {
          value = op(value, array[i]);
     }
     return value;
}

/* need to wrap the addition operator in a function so we can get a pointer to it */
int sum (int x, int y) {
     return x + y;
}

/* now combine the two parts back together to implement array sum */
int sum_array (int *array, int length) {
	return fold_int_array(array, length, 0, sum);
}

typedef struct _string_list {
     char *value;
     struct _string_list *next;
} string_list;

int size_list (string_list *list) {
     int size = 0;
     while (list) {
          size += strlen(list->value);
          list = list->next;
     }
     return size;
}

char* concat_list (string_list *list) {
     char *outbuf = malloc(size_list(list) + 1);
     if (outbuf) {
	     char *outp = outbuf;
	     while (list) {
    	      outp = stpcpy(outp, list->value);
        	  list = list->next;
	     }
     }
     return outbuf;     
}

typedef int (*string_binop)(char*, char*);

char* fold_string_list(string_list* list, char* init, string_binop op) {
	char* value = init;
	while (list) {
		value = op(value, list->value);
		list = list->next;
	}
	return value;
}

int accumulate_length(int sum, char* string) {
     return sum + strlen(string);
}

char* concat_string_list(string_list* list) {
     char *outbuf = malloc(fold_string_list(list, 0, accumulate_length) + 1);
     if (outbuf)
	     fold_string_list(list, outbuf, stpcpy);
     return outbuf;
}

char* accumulate_length_helper(char* sum, char* string) {
     return (char*) accumulate_length((int) sum, (char*) string);
}


typedef void* (*binop)(void*, void*);

int fold_array(void **array, int length, void* init, binop op) {
     void* value = init;
     for (int i = 0; i < length; i++) {
          value = op(value, array[i]);
     }
     return value;
}

typedef struct _list {
	void* value;
	struct _list* next;
} list;

void* fold_list(list* list, void* init, binop op) {
	void* value = init;
	while (list) {
		value = op(value, list->value);
		list = list->next;
	}
	return value;
}

typedef void* (*fold_fn)(void*,void*,binop);

char *concat(fold_fn fold, void * collection) {
     char *outbuf = malloc(fold(collection, 0, accumulate_length) + 1);
	 fold(collection, outbuf, stpcpy);
     return outbuf;
}

// concat(fold_list, mylist) === concat_string_list(mylist)
typedef struct _counted_array {
	void *value;
	int length;
} counted_array;

void* fold_counted_array(counted_array* array, void* init, binop op) {
	return fold_array(array->value, array->length, init, op);
}

/* Now we can write:
 * concat(fold_counted_array, my_counted_array)
 */


int main(int argc, char* argv[]) {
	string_list l2 = { "bar", NULL };
	string_list l1 = { "foo", &l2 };

	const char* str_array[] = { "hello", "world" };
	counted_array array = { str_array, 2 };
	
	assert(strcmp(concat((fold_fn) fold_list, &l1), "foobar") == 0);
	assert(strcmp(concat((fold_fn) fold_counted_array, &array), "helloworld") == 0);		
}

