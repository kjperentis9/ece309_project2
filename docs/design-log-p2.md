# Design Log — Project 2

(500–800 words total. See spec §5 for what each section must cover.)

## Growth factor and amortized cost

We start with capacity 1 and double when full. Across n appends, the messages moved during resizing form a geometric sum, 1 + 2 + 4 + ... < 2n. Combined with storing the $n$ new messages, that gives O(n) total work and O(1) amortized work per append, treating each message move as constant time.

## Rule of Five evidence

Copies allocate separate arrays. Copy assignment uses a temporary copy and swaps ownership. Moves transfer the pointer and reset the source to empty. The destructor uses delete[]. All eight initial tests passed with sanitizers enabled.

## Sentinel scanner: bounded pending_ proof



## What I would change differently
