
#include "persistence.h"
#include "skiplist_str.h"
#include "lstr.h"

int main(void) {
    SkipList *td = sl_s_test_data();
    LString *fp = lstring_create_from_buf(8, "test.sst");
    dump_memtable_to_sst_v2(td, fp);
}
