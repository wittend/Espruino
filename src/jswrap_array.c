/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2013 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * This file is designed to be parsed during the build process
 *
 * JavaScript methods for Arrays
 * ----------------------------------------------------------------------------
 */
#include "jswrap_array.h"
#include "jsparse.h"

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))


/*JSON{ "type":"class",
        "class" : "Array",
        "check" : "jsvIsArray(var)",
        "description" : ["This is the built-in JavaScript class for arrays.",
                         "Arrays can be defined with ```[]```, ```new Array()```, or ```new Array(length)```" ]
}*/

/*JSON{ "type":"constructor", "class": "Array",  "name": "Array",
         "description" : "Create an Array. Either give it one integer argument (>=0) which is the length of the array, or any number of arguments ",
         "generate" : "jswrap_array_constructor",
         "params" : [ [ "args", "JsVarArray", "The length of the array OR any number of items to add to the array" ] ],
         "return" : [ "JsVar", "An Array" ]

}*/
JsVar *jswrap_array_constructor(JsVar *args) {
  assert(args);
  if (jsvGetArrayLength(args)==1) {
    JsVar *firstArg = jsvSkipNameAndUnLock(jsvArrayGetLast(args)); // also the first!
    if (jsvIsInt(firstArg) && jsvGetInteger(firstArg)>=0) {
      JsVarInt count = jsvGetInteger(firstArg);
      // we cheat - no need to fill the array - just the last element
      if (count>0) {
        JsVar *arr = jsvNewWithFlags(JSV_ARRAY);
        if (!arr) return 0; // out of memory
        JsVar *idx = jsvMakeIntoVariableName(jsvNewFromInteger(count-1), 0);
        if (idx) { // could be out of memory
          jsvAddName(arr, idx);
          jsvUnLock(idx);
        }
        jsvUnLock(firstArg);
        return arr;
      }
    }
    jsvUnLock(firstArg);
  }
  // Otherwise, we just return the array!
  return jsvLockAgain(args);
}

/*JSON{ "type":"method", "class": "Array", "name" : "indexOf",
         "description" : "Return the index of the value in the array, or -1",
         "generate" : "jswrap_array_indexOf",
         "params" : [ [ "value", "JsVar", "The value to check for"] ],
         "return" : ["JsVar", "the index of the value in the array, or -1"]
}*/
JsVar *jswrap_array_indexOf(JsVar *parent, JsVar *value) {
  JsVar *idxName = jsvGetArrayIndexOf(parent, value, false/*not exact*/);
  // but this is the name - we must turn it into a var
  if (idxName == 0) return jsvNewFromInteger(-1); // not found!
  JsVar *idx = jsvCopyNameOnly(idxName, false/* no children */, false/* Make sure this is not a name*/);
  jsvUnLock(idxName);
  return idx;
}

/*JSON{ "type":"method", "class": "Array", "name" : "join",
         "description" : "Join all elements of this array together into one string, using 'separator' between them. eg. ```[1,2,3].join(' ')=='1 2 3'```",
         "generate" : "jswrap_array_join",
         "params" : [ [ "separator", "JsVar", "The separator"] ],
         "return" : ["JsVar", "A String representing the Joined array"]
}*/
JsVar *jswrap_array_join(JsVar *parent, JsVar *filler) {
  if (jsvIsUndefined(filler))
    filler = jsvNewFromString(","); // the default it seems
  else
    filler = jsvAsString(filler, false);
  if (!filler) return 0; // out of memory
  JsVar *str = jsvArrayJoin(parent, filler);
  jsvUnLock(filler);
  return str;
}

/*JSON{ "type":"method", "class": "Array", "name" : "push",
         "description" : "Push a new value onto the end of this array'",
         "generate" : "jswrap_array_push",
         "params" : [ [ "arguments", "JsVarArray", "One or more arguments to add"] ],
         "return" : ["int", "The new size of the array"]
}*/
JsVarInt jswrap_array_push(JsVar *parent, JsVar *args) {
  JsVarInt len = -1;
  JsvArrayIterator it;
  jsvArrayIteratorNew(&it, args);
  while (jsvArrayIteratorHasElement(&it)) {
    JsVar *el = jsvArrayIteratorGetElement(&it);
    len = jsvArrayPush(parent, el);
    jsvUnLock(el);
    jsvArrayIteratorNext(&it);
  }
  jsvArrayIteratorFree(&it);
  if (len<0) 
    len = jsvGetArrayLength(parent);
  return len;
}


/*JSON{ "type":"method", "class": "Array", "name" : "pop",
         "description" : "Pop a new value off of the end of this array",
         "generate_full" : "jsvArrayPop(parent)",
         "return" : ["JsVar", "The value that is popped off"]
}*/

JsVar *_jswrap_array_map_or_forEach(JsVar *parent, JsVar *funcVar, JsVar *thisVar, bool isMap) {
  if (!jsvIsFunction(funcVar)) {
    jsError("Array.map's first argument should be a function");
    return 0;
  }
  if (!jsvIsUndefined(thisVar) && !jsvIsObject(thisVar)) {
    jsError("Arraymap's second argument should be undefined, or an object");
    return 0;
  }
  JsVar *array = 0;
  if (isMap)
    array = jsvNewWithFlags(JSV_ARRAY);
  if (array || !isMap) {
   JsVarRef childRef = parent->firstChild;
   while (childRef) {
     JsVar *child = jsvLock(childRef);
     if (jsvIsInt(child)) {
       JsVar *args[3], *mapped;
       args[0] = jsvLock(child->firstChild);
       // child is a variable name, create a new variable for the index
       args[1] = jsvNewFromInteger(jsvGetInteger(child));
       args[2] = parent;
       mapped = jspeFunctionCall(funcVar, 0, thisVar, false, 3, args);
       jsvUnLock(args[0]);
       jsvUnLock(args[1]);
       if (mapped) {
         if (isMap) {
           JsVar *name = jsvCopyNameOnly(child, false/*linkChildren*/, true/*keepAsName*/);
           if (name) { // out of memory?
             name->firstChild = jsvGetRef(jsvRef(mapped));
             jsvAddName(array, name);
             jsvUnLock(name);
           }
         }
         jsvUnLock(mapped);
       }
     }
     childRef = child->nextSibling;
     jsvUnLock(child);
   }
  }
  return array;
}

/*JSON{ "type":"method", "class": "Array", "name" : "map",
         "description" : "Return an array which is made from the following: ```A.map(function) = [function(A[0]), function(A[1]), ...]```",
         "generate" : "jswrap_array_map",
         "params" : [ [ "function", "JsVar", "Function used to map one item to another"] ,
                      [ "thisArg", "JsVar", "if specified, the function is called with 'this' set to thisArg (optional)"] ],
         "return" : ["JsVar", "The value that is popped off"]
}*/
JsVar *jswrap_array_map(JsVar *parent, JsVar *funcVar, JsVar *thisVar) {
  return _jswrap_array_map_or_forEach(parent, funcVar, thisVar, true);
}


/*JSON{ "type":"method", "class": "Array", "name" : "splice",
         "description" : "Both remove and add items to an array",
         "generate" : "jswrap_array_splice",
         "params" : [ [ "index", "int", "Index at which to start changing the array. If negative, will begin that many elements from the end"],
                      [ "howMany", "JsVar", "An integer indicating the number of old array elements to remove. If howMany is 0, no elements are removed."],
                      [ "element1", "JsVar", "A new item to add (optional)" ],
                      [ "element2", "JsVar", "A new item to add (optional)" ],
                      [ "element3", "JsVar", "A new item to add (optional)" ],
                      [ "element4", "JsVar", "A new item to add (optional)" ],
                      [ "element5", "JsVar", "A new item to add (optional)" ],
                      [ "element6", "JsVar", "A new item to add (optional)" ] ],
         "return" : ["JsVar", "An array containing the removed elements. If only one element is removed, an array of one element is returned."]
}*/
JsVar *jswrap_array_splice(JsVar *parent, JsVarInt index, JsVar *howManyVar, JsVar *element1, JsVar *element2, JsVar *element3, JsVar *element4, JsVar *element5, JsVar *element6) {
  JsVarInt len = jsvGetArrayLength(parent);
  if (index<0) index+=len;
  if (index<0) index=0;
  if (index>len) index=len;
  JsVarInt howMany = len; // how many to delete!
  if (jsvIsInt(howManyVar)) howMany = jsvGetInteger(howManyVar);
  if (howMany > len-index) howMany = len-index;
  JsVarInt newItems = 0;
  if (element1) newItems++;
  if (element2) newItems++;
  if (element3) newItems++;
  if (element4) newItems++;
  if (element5) newItems++;
  if (element6) newItems++;
  JsVarInt shift = newItems-howMany;

  bool needToAdd = false;
  JsVar *result = jsvNewWithFlags(JSV_ARRAY);

  JsvArrayIterator it;
  jsvArrayIteratorNew(&it, parent);
  while (jsvArrayIteratorHasElement(&it) && !needToAdd) {
    bool goToNext = true;
    JsVar *idxVar = jsvArrayIteratorGetIndex(&it);
    if (idxVar && jsvIsInt(idxVar)) {
      JsVarInt idx = jsvGetInteger(idxVar);
      if (idx<index) {
        // do nothing...
      } else if (idx<index+howMany) { // must delete
        if (result) { // append to result array
          JsVar *el = jsvArrayIteratorGetElement(&it);
          jsvArrayPushAndUnLock(result, el);
        }
        // delete
        goToNext = false;
        JsVar *toRemove = jsvArrayIteratorGetIndex(&it);
        jsvArrayIteratorNext(&it);
        jsvRemoveChild(parent, toRemove);
        jsvUnLock(toRemove);
      } else { // we're greater than the amount we need to remove now
        needToAdd = true;
        goToNext = false;
      }
    }
    jsvUnLock(idxVar);
    if (goToNext) jsvArrayIteratorNext(&it);
  }
  // now we add everything
  JsVar *beforeIndex = jsvArrayIteratorGetIndex(&it);
  if (element1) jsvArrayInsertBefore(parent, beforeIndex, element1);
  if (element2) jsvArrayInsertBefore(parent, beforeIndex, element2);
  if (element3) jsvArrayInsertBefore(parent, beforeIndex, element3);
  if (element4) jsvArrayInsertBefore(parent, beforeIndex, element4);
  if (element5) jsvArrayInsertBefore(parent, beforeIndex, element5);
  if (element6) jsvArrayInsertBefore(parent, beforeIndex, element6);
  jsvUnLock(beforeIndex);
  // And finally renumber
  while (jsvArrayIteratorHasElement(&it)) {
      JsVar *idxVar = jsvArrayIteratorGetIndex(&it);
      if (idxVar && jsvIsInt(idxVar)) {
        jsvSetInteger(idxVar, jsvGetInteger(idxVar)+shift);
      }
      jsvUnLock(idxVar);
      jsvArrayIteratorNext(&it);
    }
  // free
  jsvArrayIteratorFree(&it);

  return result;
}


/*JSON{ "type":"method", "class": "Array", "name" : "slice",
         "description" : "Return a copy of a portion of the calling array",
         "generate" : "jswrap_array_slice",
         "params" : [ [ "start", "JsVar", "Start index"],
                      [ "end", "JsVar", "End index (optional)"] ],
         "return" : ["JsVar", "A new array"]
}*/
JsVar *jswrap_array_slice(JsVar *parent, JsVar *startVar, JsVar *endVar) {
  JsVarInt len = jsvGetArrayLength(parent);
  JsVarInt start = 0;
  JsVarInt end = len;

  if (!jsvIsUndefined(startVar))
    start = jsvGetInteger(startVar);

  if (!jsvIsUndefined(endVar))
    end = jsvGetInteger(endVar);

  JsVarInt k = 0;
  JsVarInt final = len;
  JsVar *array = jsvNewWithFlags(JSV_ARRAY);

  if (!array) return 0;

  if (start<0) k = max((len + start), 0);
  else k = min(start, len);

  if (end<0) final = max((len + end), 0);
  else final = min(end, len);

  bool isDone = false;

  JsvArrayIterator it;
  jsvArrayIteratorNew(&it, parent);

  while (jsvArrayIteratorHasElement(&it) && !isDone) {
    JsVarInt idx = jsvGetIntegerAndUnLock(jsvArrayIteratorGetIndex(&it));

    if (idx < k) {
      jsvArrayIteratorNext(&it);
    } else {
      if (k < final) {
        jsvArrayPushAndUnLock(array, jsvArrayIteratorGetElement(&it));
        jsvArrayIteratorNext(&it);
        k++;
      } else {
        isDone = true;
      }
    }
  }

  jsvArrayIteratorFree(&it);

  return array;
}


/*JSON{ "type":"method", "class": "Array", "name" : "forEach",
         "description" : "Executes a provided function once per array element.",
         "generate" : "jswrap_array_forEach",
         "params" : [ [ "function", "JsVar", "Function to be executed"] ,
                      [ "thisArg", "JsVar", "if specified, the function is called with 'this' set to thisArg (optional)"] ]
}*/
void jswrap_array_forEach(JsVar *parent, JsVar *funcVar, JsVar *thisVar) {
  _jswrap_array_map_or_forEach(parent, funcVar, thisVar, false);
}

/*JSON{ "type":"staticmethod", "class": "Array", "name" : "isArray",
         "description" : "Returns true if the provided object is an array",
         "generate_full" : "jsvIsArray(var)",
         "params" : [ [ "var", "JsVar", "The variable to be tested"] ],
         "return" : ["bool", "True if var is an array, false if not."]
}*/


NO_INLINE static bool _jswrap_array_sort_leq(JsVar *a, JsVar *b, JsVar *compareFn) {
  if (compareFn) {
    JsVar *args[2] = {a,b};
    JsVarInt r = jsvGetIntegerAndUnLock(jspeFunctionCall(compareFn, 0, 0, false, 2, args));
    return r<0;
  } else {
    return jsvGetBoolAndUnLock(jsvMathsOp(a,b,LEX_LEQUAL));
  }
}

NO_INLINE static void _jswrap_array_sort(JsvIterator *head, int n, JsVar *compareFn) {
  if (n < 2) return; // sort done!

  JsvIterator pivot = jsvIteratorClone(head);
  JsVar *pivotValue = jsvIteratorGetValue(&pivot);
  /* We're just going to use the first entry (head) as the pivot...
   * We'll move along with our iterator 'it', and if it < pivot then we'll
   * swap the values over (hence moving pivot forwards)  */

  int nlo = 0, nhigh = 0;
  JsvIterator it = jsvIteratorClone(head); //
  jsvIteratorNext(&it);

  /* Partition and count sizes. */
  while (--n && !jspIsInterrupted()) {
    JsVar *itValue = jsvIteratorGetValue(&it);
    if (_jswrap_array_sort_leq(itValue, pivotValue, compareFn)) {
      nlo++;
      /* 'it' <= 'pivot', so we need to move it behind.
         In this diagram, P=pivot, L=it

               l l l l l P h h h h h L
                         |  \       /
                          \  \_____/_
                          _\______/  \
                         / |         |
                         | |         |
               l l l l l L P h h h h h

         It makes perfect sense now...
      */
      // first, get the old pivot value and overwrite it with the iterator value
      jsvIteratorSetValue(&pivot, itValue); // no unlock needed
      // now move pivot forwards, and set 'it' to the value the new pivot has
      jsvIteratorNext(&pivot);
      jsvUnLock(jsvIteratorSetValue(&it, jsvIteratorGetValue(&pivot)));
      // finally set the pivot iterator to the pivot's value again
      jsvIteratorSetValue(&pivot, pivotValue); // no unlock needed
    } else {
      nhigh++;
      // Great, 'it' > 'pivot' so it's in the right place
    }
    jsvUnLock(itValue);
    jsvIteratorNext(&it);
  }
  jsvIteratorFree(&it);
  jsvUnLock(pivotValue);

  if (jspIsInterrupted()) return;

  // now recurse
  _jswrap_array_sort(head, nlo, compareFn);
  jsvIteratorNext(&pivot);
  _jswrap_array_sort(&pivot, nhigh, compareFn);
  jsvIteratorFree(&pivot);
}

/*JSON{ "type":"method", "class": "Array", "name" : "sort", "ifndef" : "SAVE_ON_FLASH",
         "description" : "Do an in-place quicksort of the array",
         "generate" : "jswrap_array_sort",
         "params" : [ [ "var", "JsVar", "A function to use to compare array elements (or undefined)"] ],
         "return" : [ "JsVar", "This array object" ]
}*/
JsVar *jswrap_array_sort (JsVar *array, JsVar *compareFn) {
  if (!jsvIsUndefined(compareFn) && !jsvIsFunction(compareFn)) {
    jsError("Expecting compare function, got %t", compareFn);
    return 0;
  }
  JsvIterator it;

  /* Arrays can be sparse and the iterators don't handle this
    (we're not going to mess with indices) so we have to count
     up the number of elements manually.

     FIXME: sort is broken for sparse arrays anyway (it basically
     ignores all the 'undefined' entries). I wonder whether just
     compacting the array down to start from 0 before we start would
     fix this?
   */
  int n=0;
  if (jsvIsArray(array) || jsvIsObject(array)) {
    jsvIteratorNew(&it, array);
    while (jsvIteratorHasElement(&it)) {
      n++;
      jsvIteratorNext(&it);
    }
    jsvIteratorFree(&it);
  } else {
    n = (int)jsvGetLength(array);
  }

  jsvIteratorNew(&it, array);
  _jswrap_array_sort(&it, n, compareFn);
  jsvIteratorFree(&it);
  return jsvLockAgain(array);
}

/*JSON{ "type":"method", "class": "Array", "name" : "concat", "ifndef" : "SAVE_ON_FLASH",
         "description" : "Create a new array, containing the elements from this one and any arguments, if any argument is an array then those elements will be added.",
         "generate" : "jswrap_array_concat",
         "params" : [ [ "args", "JsVarArray", "Any items to add to the array" ] ],
         "return" : [ "JsVar", "An Array" ]

}*/
JsVar *jswrap_array_concat(JsVar *parent, JsVar *args) {
  JsVar *result = jsvNewWithFlags(JSV_ARRAY);

  JsVar *source = jsvLockAgain(parent);

  JsvArrayIterator sourceit;
  jsvArrayIteratorNew(&sourceit, args);
  while (source) {
    if (jsvIsArray(source)) {
      JsvArrayIterator it;
      jsvArrayIteratorNew(&it, parent);
      while (jsvArrayIteratorHasElement(&it)) {
        jsvArrayPushAndUnLock(result, jsvArrayIteratorGetElement(&it));
        jsvArrayIteratorNext(&it);
      }
      jsvArrayIteratorFree(&it);
    } else
      jsvArrayPush(result, source);
    // next
    jsvUnLock(source);
    source = jsvArrayIteratorHasElement(&sourceit) ? jsvArrayIteratorGetElement(&sourceit) : 0;
    jsvArrayIteratorNext(&sourceit);
  }
  jsvArrayIteratorFree(&sourceit);
  return result;
}
