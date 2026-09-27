# Design Log — Project 2

(500–800 words total. See spec §5 for what each section must cover.)

## Growth factor and amortized cost

My Conversation class starts with capacity 0 and allocates space for one message on the first append. Whenever the array becomes full, its capacity doubles. This produces capacities of 1, 2, 4, 8, and so on. Doubling gives the conversation room to grow without allocating a new array every time a message is added.

An append that has available space takes O(1) work, treating each message operation as constant time. An append that requires resizing takes O(n) work because the existing messages must be transferred to the larger array. However, resizing does not happen on every append. Across n appends, the number of existing messages transferred follows the geometric sum 1 + 2 + 4 + ..., which is less than 2n. Including the n new messages gives O(n) total work and O(1) amortized work per append.

The tradeoff is unused capacity after resizing. I chose doubling because it keeps the implementation straightforward while reducing how often allocations happen. My growth test checks the capacity after every append, verifies that 100 messages produce a capacity of 128, and confirms that all message contents remain correct.

## Rule of Five evidence

Conversation owns a dynamically allocated array, so it needs to manage copying, moving, and deletion correctly. The destructor releases the array with delete[]. The copy constructor allocates separate storage and copies the messages, allowing both conversations to exist independently.

Copy assignment uses a temporary copy and swaps ownership. This replaces the destination’s previous contents while allowing the temporary object to clean up its old array. It also supports self-assignment. The move constructor and move assignment transfer ownership of the existing array instead of copying every message. They reset the source to an empty state so it no longer owns the transferred storage.

My tests check that copied conversations have different array addresses and preserve their contents independently. They also check copy assignment into an existing conversation. The move tests verify that both move construction and move assignment transfer the original array address and reset the source pointer, size, and capacity to zero.

All 13 tests passed when I ran the test executable with leak detection enabled. These tests provide evidence for the behaviors checked, although passing them does not guarantee that every possible case is covered.

## Sentinel scanner: bounded pending_ proof

The scanner combines pending text from the previous call with the next chunk before searching for the sentinel. This allows it to detect a marker split across chunk boundaries. If it finds the sentinel, it returns only the text before the marker, clears pending_, and records that scanning has stopped. Later calls return no additional text.

Let m be the sentinel’s length. After a call that does not find the sentinel, the scanner retains at most m − 1 trailing characters. If the combined text contains at most that many characters, it keeps all of them. Otherwise, it emits the earlier text and retains exactly the final m − 1 characters. A successful match leaves pending_ empty, so every case satisfies the bound.

Keeping these characters is sufficient because an incomplete sentinel can contain at most m − 1 characters before the next chunk arrives. The marker <|end_conversation|> has 20 characters, so pending_ stores at most 19. The temporary combined string also holds the current chunk; the scanner does not retain the entire response across calls.

The tests cover every two-chunk split, one-character chunks, ordinary text, similar markers, and incomplete markers. A 4 MiB test checks the pending size after every byte and verifies the emitted characters. Finally, flush() releases any remaining ordinary text when the stream ends.

## What I would change differently

If I started again, I would add smaller tests alongside each function instead of expanding the tests after implementing the main classes. That would make it easier to connect a failed assertion to a specific change.

I would also consider retaining only the longest trailing sequence that matches the beginning of the sentinel. The current approach can delay ordinary text by holding up to 19 characters even when they do not resemble the marker. A more selective approach could return text sooner, but it would require additional matching logic and tests. For this project, the current method was easier to explain and verify.