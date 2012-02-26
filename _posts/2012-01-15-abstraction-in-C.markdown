---
layout: post
category : programming
---
This is the first in a series of posts exploring forms of abstraction supported by various 
programming languages, from the primitive to the advanced.  First up: C.

C is not known for being abstract, for good reason.  It translates to assembly language in a rather 
straightforward way.  It's certainly not going to perform dynamic dispatch or garbage collection for
you.  It has no equivalent to the templates of C++ or the functors of ML for writing generic code.
So let's see what happens when we attempt to write high-level, generic code with it.

The problem:  sum the elements in a collection.  The collection is abstract, and the value type is
abstract as well.  We might be summing an array of ints, or a linked list
of doubles.  And by "sum", I really just mean perform a binary operation of some sort, so we could
also be concatenating a list of strings.  

In Haskell, this is straightforward:

{% highlight haskell %}
-- sum this list of integers:
foldl (+) 0 [1,2,3,4]

-- create an array of strings 
let a = array (1,2) [(1, "foo"), (2, "bar")]
-- concatenate the array (using monoids):
fold a
{% endhighlight %}


Let's see what happens when we try to do something like this in C.

First, a simple, non-generic array sum:

{% highlight c %}
int sum_array_direct (int *xs, int length) {
     int sum = 0;
     for (int i = 0; i < length; i++)
          sum += xs[i];
     return sum;
}
{% endhighlight %}

Now for the abstract version, we'll decompose the function into two parts:  one which performs 
the iteration, and the other which performs the sum.

{% highlight c %}
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
{% endhighlight %}

Great!  We've tripled the size of our code!  Of course, we wouldn't really have to define the 
sum_array function since `fold_int_array (array, length, 0, sum)` is already pretty short, and we
should see savings later.

Moving on to a more complex example: list concatenation. 
List concatentation gets a little more complex due to the manual memory management: we 
traverse the list twice, first to figure out how much memory to allocate, then to actually
copy the data into the new buffer.  Again, starting with the direct
implementation:

{% highlight c %}
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
{% endhighlight %}

Here we can start to see some common structure.  Let's see what it looks like when we implement it
using folds.  We have two folds, corresponding to the two list traversals in the direct implementation.
The `stpcpy` function from GNU libc conveniently works perfectly as the fold operation for the
concatenation; we just need a helper function to accumulate the length.  Here goes:

{% highlight c %}
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

{% endhighlight %}

It's the same size as the original, even considering the overhead of defining the fold function!
But now the compiler's not quite happy:  we get some rather ugly warnings.
We've taken a step into the vast wilderness of undefined behavior, which is not somewhere I like 
to be.
The C compiler dutifully warns us that our types are not kosher, and indeed, if we run this on a system
where integers and pointers are not the same size, it will crash.  We can improve things
with a helper function:

{% highlight c %}
char* accumulate_length_helper(char* sum, char* string) {
     return (char*) accumulate_length((int) sum, (char*) string);
}
{% endhighlight %}

Now, instead of simply throwing the arguments on the stack, in a way which the receiving function
may or may not expect, we add explicit casts which tell the compiler to make the values the correct 
size.  For this to work, we need to be on an architecture where pointers are at least as large as 
ints, or else data will be lost in the cast.  That is generally the case on today's architectures, 
but it's still rather...unclean.  Anyway, pressing on.

We now have a way to traverse int arrays, and a way to traverse string lists.  But what about
string arrays, or int lists, or any of the myriad other combinations of data structure and value type?

Since we've already ventured into the realm of unchecked casts, we might as well generalize the above
definitions, and introduce one of C's most idiosyncratic features: the void pointer.  It's essentially
a find-replace of the above functions with `void*` instead of `int` or `char*`:

{% highlight c %}

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

{% endhighlight %}

It's simple enough, and unlike casting between pointers and integers, it's commonly accepted practice in C.  
We've achieved flexibility on the value type axis:  we can fold over lists
of strings, lists of ints, arrays of strings, or arrays of ints.

But our abstraction is not yet composable.

Note that string list concatenation required two folds.  To concatenate an array of strings, we'll
need to rewrite the concatenation function, this time using the array fold function instead of the
list fold function. But this is duplicative, and does not scale to larger operations.  

Fortunately, we can easily solve the problem with function pointers:

{% highlight c %}
typedef void* (*fold_fn)(void*,void*,binop);

char *concat(fold_fn fold, void * collection) {
     char *outbuf = malloc(fold(collection, 0, accumulate_length) + 1);
	 fold(collection, outbuf, stpcpy);
     return outbuf;
}

// concat(fold_list, mylist) === concat_string_list(mylist)

{% endhighlight %}

There's a small problem:  fold_array and fold_list have different signatures.  Arrays in C resist 
abstraction:  since they're not self-contained, another parameter is 
needed for the length.  Let's make a struct to wrap that up:

{% highlight c %}
typedef struct _counted_array {
	void **value;
	int length;
} counted_array;

void* fold_counted_array(counted_array* array, void* init, binop op) {
	return fold_array(array->value, array->length, init, op);
}

/* Now we can write:
 * concat(fold_counted_array, my_counted_array)
 */

{% endhighlight %}

What we have here is essentially a manual version of Haskell's typeclasses.  In Haskell, concat
would have the signature `concat :: (Foldable t) => t String -> String`, meaning that it takes 
a foldable collection of strings, and returns a string.  In Haskell, the `(Foldable t) =>` part of 
the signature causes the compiler to find the appropriate fold function for the type `t` and 
supply it behind-the-scenes, whereas in C we have to manually specify it.  Note that our C version 
takes two arguments, and the Haskell version takes one argument, but the type signature hints at 
the additional second argument which is supplied for us.

(Bonus question: how is this different from the typical "objects in C" pattern used by e.g. the Linux
kernel and GTK?)

So, what have we learned?  First, it is possible to program in a somewhat-functional style using C,
and in some cases, to gain some advantage from doing so.  Abstraction in C is _possible_.

But it's not pretty.  The code will produce tons of warnings, some of which are benign, some of which
aren't, and it's up to us to know which is which.  We can insert casts to suppress the warnings, but 
this just hides the problem.  As a result, it's easy to make an invalid cast, and the compiler will 
not offer any help.  We really have to know if it's okay to use, say, a `void*`
as an `int` on our architecture, and every architecture we target.

We also have been lucky that memory management did not come up.  The values returned by the fold 
operation in this example are either ints or pointers into preallocated memory.  What if we needed
to return an intermediate value of a struct type?  We could dynamically allocate it and 
return the pointer from the fold operation, but then we have a memory leak.  We could write a different fold, which
frees the intermediate values, but that doubles the number of fold functions we need to write.  There's
simply no easy solution in standard C.

But perhaps if we had some sort of preprocessor, or maybe a templating language?  Hmm...
