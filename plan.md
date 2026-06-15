Ideas:

Serialization/Deser
- add code generator (e.g. Python) that autogens C serialization and deserialization code based on a language
- can also use templates of C code and only put in certain blocks
- e.g. format for block, format for metablocks, headers etc and autogens C code



big questions:

1. when reading from disk, how much should we read? should we read entire file and work in-memory always?
e.g. let's say I want to search for a key in an SST file, should I just deserialize the SST file into DataSection + IndexSection and then do search there? I guess it's better to first deserialize the index, find the block you need, and deserialize that! I guess that makes more sense.



2026-06-14

- data blocks have to have a footer at the end for users to know where to jump
to find the restart points (actually count is enough since elements are fixed
size)

- big discussion:
  - currently for BlockItem we do the following
  - during creation of BlockItems when we're serializing the keys to file
    we take the original allocated keys and we take slices into them for the serialization
    and we destroy everything at the end; specifically we destroy the skiplist in `server.c`
    via sl_s_destroy
    - basically look at the call tree:
      - persistence.c:compress_and_write
      - persistence.c:dump_memtable_to_sst
      - server.c:dump_memtable_to_sstable
  - then during deserialization we reconstruct `BlockItems` and this time again we do not
    allocate keys, but instead we use the contiguous `decompressed` memory block and again
    we use Slices/Views into this for `suffix` and `value`
  - the key is to never lose the original pointer backing the slice/view because we need
    to deallocate it later. so for serialization the original pointers live in the memtable/skiplist
    and or deserialization it means we must keep the `decompressed` pointer alive to free it later
  - there is one "inefficiency" in that the `u32` in `BlockItem` are `memcpy`-ed but the strings
    are slices, so if we keep the `decompressed` buffer alive we are wasting memory




