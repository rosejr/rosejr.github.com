---
title: "Abstraction, Continued"
layout: post
category : programming
---
The previous article focused on a particular use case for abstraction&colon;  can we define a function which operates on any collection?  Of course, we want our collections to be generic in the element type as well.  We studied this problem in the context of C previously, and it fared rather poorly.  Here, we'll see how other languages compare.

To make the problem more concrete, let's say we want a function which calculates statistical variance over any collection of numbers.  To calculate the variance, we must first calculate the mean, then calculate the difference of each element from the mean, then sum the squares of these differences.  Note that, while collections should be generic, this function requires that the collections passed to it hold numbers.  Here's an implementation in Haskell:

{% highlight haskell %}
variance :: (Foldable s, Fractional t) => s t -> t
variance s = let count = foldr (\_ -> (+1)) 0 s    
                 mean = (sum s) / count             
                 sumdiff sum x = sum + (mean - x)^2
              in foldl sumdiff 0 s
{% endhighlight %}

In C++, the problem becomes almost as easy as in Haskell, due to the STL:

{% highlight c++ %}
struct SumSquareDist {
     double from;
     SumSquareDist(double f) : from(f) {}
     double operator()(double sum, double x) {   
          return sum + (from - x) * (from - x);
     }
};
template <typename Coll>
double variance(Coll c) {
     double mean = std::accumulate(c.begin(), c.end(), 0.0) / c.size();
     return std::accumulate(c.begin(), c.end(), 0.0, SumSquareDist(mean));
}
{% endhighlight %}	

We can vary both the type of the collection and the operation.  Anything that has iterators, we can use with the pre-built algorithms from the STL, which include the typical map and filter, as well as things like shuffles and permutations.

C++ is advertised as a "multi-paradigm" language though.  So what if we try to do it without the help of templates, using one of C++'s other paradigms?  Is OO enough to perform this type of abstraction?  Does the OO support in C++ buy us anything at all over what we had with C?

It turns out, not much.  Without templates, we are still forced into using void pointers to obtain polymorphism.  The only thing that we gain is that the fold function can be a method on the container class, which saves some typing and eliminates a source of errors.  But none of the core issues are resolved.  We still have to perform casts for all operations.  

Java, pre-generics, suffers from the same issue.  This example can be translated directly to Java 1.4, and it will still need all of the casts and it will still suffer the same problems (except of course that bad casts result in exceptions rather than arbitrary behavior.)

So, what exactly are the necessary and sufficient language features needed to support this?  Of course, if we ditch the types it becomes easy, but that's only an option if you're willing to live with the compromises entailed by dynamic languagues.  We've seen that templates can be used to programmatically generate type-safe code.  Haskell's type system, with parametric polymorphism and type classes (a form of ad-hoc polymorphism), does the trick, but the subtype-polymorphism provided by objects is not enough by itself.  

It turns out though that the addition of generics (parametric polymorphism) to Java is sufficient:

{% highlight java %}
interface Function2<A1,A2,R> {
     R apply(A1 a1, A2 a2);
}
interface Foldable<Elt> {
     R foldLeft<R>(R init, Function2<R, Elt, R> op);
     R foldRight<R>(R init, Function2<Elt, R, R> op);
}

// Some operations elided for brevity...
public static double variance(Foldable<Double> collection) {
	int length = collection.foldLeft(0, add1);
	final double mean = collection.foldLeft(0.0, doubleAdd) / length;
	return collection.foldLeft(0.0, new Function2<Double, Double, Double>() {
		@Override public Double apply(Double sum, Double x) {
			return sum + (mean - x) * (mean - x);
		}
	});
}
{% endhighlight %}	


What's more surprising, though, is how awkward this is in ML.  There is no direct equivalent to Haskell's type classes.  We have objects in OCaml, but not in Standard ML.  Parametric polymorphism is well supported, but it is not enough by itself:  parametric polymorphism generalizes to *all* types, and we only want certain types (those which are foldable collections).  The module system is generally considered to be the feature most comparable to Haskell's type classes, so let's see what happens when we try to use it:

{% highlight ocaml %}

(* Define a module type for foldable collections *)
module type FOLDABLE = sig
     type 'a collection
     val foldl : ('b -> 'a -> 'b) -> 'b -> 'a collection -> 'b
end;;
    
(* Define a module instance for lists -- this is how we say "lists are foldable" *)
module FoldableList =
struct
     type 'a collection = 'a list
     let foldl = List.fold_left
end;;

(* We'd like to define a top-level function "variance" but we can't so we put it in a functor. 
 * The functor has a type which only allows modules representing foldable collections to be supplied.
 * The functor produces a module which contains our function.
 *)
module Variance = functor (Collection : FOLDABLE) -> struct
     let variance (coll : float Collection.collection) =
          let count = Collection.foldl (fun x -> fun _ -> x + 1) 0 coll in
          let mean = Collection.foldl (+.) 0. coll /. (float count) in
          let sumdiff sum x = sum +. (mean -. x) ** 2.
          in Collection.foldl sumdiff 0.0 coll
end;;

(* Tie together our variance function with the FoldableList instance *)
module VarianceList = Variance(FoldableList);;

(* Now we can finally call our variance function. *)
VarianceList.variance [1.;4.;7.;23.0];;
{% endhighlight %}	


Unfortunately, this is sub-optimal.  We still don't have a single function which accepts any collection and computes variance.  Instead we have a factory for creating variance functions:  it saves time but we still have to turn the crank for each one.  That means a fair bit of boilerplate, though perhaps it's not as bad as what we need in Java to pass functions around.

There are composability concerns as well:  if we want to write another generic function which uses this variance function, then it also must be wrapped in a functor, and it must create a Variance module:

{% highlight ocaml %}
module Skew = functor (Collection : FOLDABLE) -> struct
     module Var = Variance(Collection)
     let skew (coll : float Collection.collection) = ...
end;;
{% endhighlight %}	

Once we start wrapping every function in a module, we have to instantiate a *lot* of modules.  Every function-module must instantiate all of its direct function-module dependencies.  At this point, it's clear that we're working against the language, not with the language.  Modules were designed for, well, modularity; it just so happens that they also contain the machinery for type constraints that we need here.

Something that's been in the background that I have not yet brought up:  the numeric type.  Notice that the Haskell version refers to an additional type class "Fractional".  It turns out that the Haskell implementation is using ad-hoc polymorphism not just for the container type, but also for the numeric type.  We can do our math with single- or double-precision floating point, or exact rational arithmetic, without writing new code.  What about the other languagues?  C++ can accommodate this using an additional template parameter.  For ML and Java, we would essentially need to emulate Haskell's type class, and pass it manually as another parameter.  

Although Haskell seems to come out ahead in this exercise, there is one point where Haskell is a bit lacking:  it doesn't really have a collection type class.  While the Foldable class was sufficient for this exercise, it wasn't really optimal:  note how in order to find the size of the collection, we had to iterate over the entire collection counting elements one by one.  This is verbose, and inefficient for many collection implementations.  Also, Foldable only gives you the ability to reduce a collection, not to transform or construct it.  It would have been more natural to define variance using "sum $ map (\x -> (x - mean) ^ 2) collection", but we can't implement map in terms of fold, not without a Monoid instance anyway.

So perhaps the real winner in this exercise should be Scala:

{% highlight scala %}	
def variance[N](xs : Iterable[N])(implicit n : Fractional[N]) = {
     import math.Fractional.Implicits._
     val mean = xs.sum / n.fromInt(xs.size)
     xs map { x => (x - mean) * (x - mean) } sum
}
{% endhighlight %}	

It has better support out-of-the-box for abstract programming with collections than Haskell, and it also retains Haskell's static checking and numeric-type flexibility.  Note that whereas Haskell uses type classes for both, Scala abstracts over the collection with object subtyping, and over the numeric type  via implicits (Scala's analogue to type classes).







