# SST Format


## Prefix Encoding

The idea of restart points to make search faster

I want to store keys with a shared-prefix format, thus:
| keys      | stored   |
| --------- | -------- |
| myxa      | 0 myxa   |
| myxophyta | 3 ophyta |
| myxopod   | 5 od     |
| nab       | 0 nab    |
| nabbed    | 3 bed    |
| nabbing   | 4 ing    |
| nabit     | 3 it     |
| nabk      | 3 k      |
| nabob     | 3 ob     |
| nacarat   | 2 carat  |
| nacelle   | 3 elle   |


So we always compare to the shared part with the previous key, not the ROOT
key! Even though previous itself is not stored in its entirty since it itself
depends on knowing what came before, and so on until we get to the 0 shared
part!

so we store of course

`<shared><not-shared><suffix>`
or alternatively `<shared_prefix_len><unique_suffix_len><unique_suffix>`

so concretely
| keys      | stored     |
| --------- | ---------- |
| myxa      | 0 4 myxa   |
| myxophyta | 3 6 ophyta |
| myxopod   | 5 2 od     |
| nab       | 0 3 nab    |
| nabbed    | 3 3 bed    |
| nabbing   | 4 3 ing    |
| nabit     | 3 2 it     |
| nabk      | 3 1 k      |
| nabob     | 3 2 ob     |
| nacarat   | 2 5 carat  |
| nacelle   | 3 4 elle   |


We call restart points those points where `shared==0`. So then there is
a natural restart point at `nab` after `myx*` stuff.

So we store in the array for restart points

`myxa @byte-offset-in-file`
`nab @byte-offset-in-file`

at the very least.

But we have some artificial restart points in case let's say everything shares
a prefix. Alternatively one can have full artifical restart points to get "equidistance"
between keys, based on length maybe, since some keys can be longer too!

### Reconstruction

If we're looking at NABBING aka 4 3 ing, we can reconstruct NABBING from:
* keeping previous stored, aka `nabbed`
* taking 4 of it `nabb` and adding 3 of it `ing` so `nabbing`

### Search

We do binary search at restart points (since restart points store full string)
and once we find the point we do linear search!

Note that if we're dropped at a random entry in the list that has `shared>0`, we
cannot find what the key is without linear search backwards until we reach
`shared=0` because of this recursive dependency on previous key.


## Compression

The above is basically detailing how a block is stored. Once a block is constructed as such, the entire block
is compressed using some compression algorithm. So technically the SST stores the following:

`[compressed-data-block-1][compressed-data-block-2][compressed-data-block-3]...`
and then
we have an index with 1 entry per block containing where each block starts (compressed)
index should technically contain the first key in that block to improve search

my q is why do we compress blocks individually, why not compress the whole list of blocks? actually probably
that's what should be done


Interestingly, in the index, you can store more data:

- you can store not 1 entry per block, but N entries per block, where N is the # of restart points in that block.
- then, for search, you can find which restart point of which block you're at
- then you decompress block, go to restart point, do linear search
- i.e. you can do the binary search within block already at index phase instead of later
- idk if this has advantages in terms of speed (maybe due to locality of data?)
- it has disadvantages b/c index is now larger, versus restart points being compressed


## Interfaces

```c
struct BlockItem {
  u16 shared;
  u16 suffix_len;
  u8 *suffix;
  u16 value_len;
  u8 *value;
};

struct RestartPoint {
  LString key;
  u32 offset;
};

struct Block {
  Array *items; // type BlockItem
  Array *restart_points; // type RestartPoint
  int checksum;
  // other metadata
};

LString *Block_compress(struct Block *block);
void Block_append_item(struct Block *block, struct BlockItem *item); // implicitly manages RestartPoints !
void Block_append_key(struct Block *block, struct LString *key, struct LString *prev); // nice helper
LString* Block_to_LString(struct Block *block);

struct ByteBuffer {
  // represents a byte buffer with methods to append to it without having
  // to manage ptr increment
};

void ByteBuffer_append_lstr(struct ByteBuffer *bb, LString *lstr);
void ByteBuffer_append(struct ByteBuffer *bb, u8 *data, u32 n);
void ByteBuffer_append_to_file(struct ByteBuffer *bb, FILE* fptr);

// then we can at the end do
LString *compressed_block = Block_compress(block);
ByteBuffer_append(bb, compressed_block);
lstring_free(compressed_block);
ByteBuffer_append_to_file(bb, sst_file);

```
