

Quartz notes

-----------------------------------------------

I'm not sure about the name 'Btree' that runs through all this, since the fact
that it is all implemented as a B-tree is surely irrelevant. I have not been
able to think of a better name though ...

Some of the constants mentioned below depend upon a byte being 8 bits, but this
assumption is not built into the code.

Keys and tags
-------------

Thinking of 'byte' having type 'unsigned char', a key and a tag are both
sequences of bytes. The B-tree is a repository for key-tag pairs. A key can be
looked up to find its corresponding tag. If a key is deleted, the corresponding
tag ia deleted. And in the B-tree keys are unique, so if a key-tag pair is
added in and the key is already in the Btree, the tag to be added in replaces
the tag in the B-tree.

In the B-tree key-tag pairs are ordered, and the order is the ASCII collating
order of the keys. Very precisely, if key1 and key2 point to keys with lengths
key1_len, key2_len, key1 is before/equal/after key2 according as the following
procedure returns a value less than, equal to or greater than 0,


static int compare_keys(byte * key1, int key1_len, byte * key2, int key2_len)
{
    int smaller = key1_len < key2_len ? key1_len : key2_len;
    int i;
    for (i = 0; i < smaller; i++) {
        int diff = (int) key1[i] - key2[i];
        if (diff != 0) return diff;
    }
    return key1_len - key2_len;
}

[This is okay, but none of the code fragments below have been checked.]

Any large-scale operation on the B-tree will run very much faster when the keys
have been sorted into ASCII collating order. This fact is critical to the
performance of the B-tree software.

A key-tag pair is called an 'item'. The B-tree consists therefore of a list of
items, ordered by their keys:

    I1  I2  ...  I(j-1)  Ij  I(j+1)  ...  I(n-1)  In

Item Ij has a 'previous' item, I(j-1), and a 'next' item, I(j+1).

When the B-tree is created, a single item is added in with null key and null
tag. This is the 'null item'. The null item may be searched for, and it
possible, although perhaps not useful, to replace the tag part of the null
item. But the null item cannot be deleted, and an attempt to do so is merely
ignored.

A key must not exceed 252 bytes in length, and will have a smaller upper limit
if the block size of the B-tree is less than 1086 bytes.

A tag may have length zero. There is an upper limit on the length of a tag, but
it is quite high. Roughly, the tag is divided into items of size L - kl, where
L is a a few bytes less than a quarter of the block size, and kl is length of
its key. You can then have 64K such items. So even with a block size as low as
2K and key length as large as 100, you could have a tag of 2.5 megabytes. More
realistically, with a 16K block size, the upper limit on the tag size is about
256 megabytes.


Revision numbers
----------------

The B-tree has a revision number, and each time it is updated, the revision
number increases. In a single transaction on the B-tree, it is first opened,
its revision number, R is found, updates are made, and then the B-tree is
closed with a supplied revision number. The supplied revision number will
typically be R+1, but any R+k is possible, where k > 0.

If this sequence fails to complete for some reason, revision R+k of the B-tree
will not, of course, be brought into existence. But revision R will still
exist, and it is that version of the B-tree that will be the starting point for
later revisions.

If this sequence runs to a successful termination, the new revision, R+k,
supplants the old revision, R. But it is still possible to open the B-tree at
revision R. After a successful revision of the B-tree, in fact, it will have
two valid versions: the current one, revision R+k, and the old one, revision R.

You might want to go back to the old revision of a B-tree if it is being
updated in tandem with second B-tree, and the update on the second B-tree
fails. Suppose B1 and B2 are two such B-trees. B1 is opened and its latest
revision number is found to be R1. B2 is opened and its latest revision number
is found to be R2. If R1 > R2, it must be the case that the previous
transaction on B1 succeeded and the previous transaction on B2 failed. Then B1
needs to opened at its previous revision number, which must be R1.

The calls using revision numbers described below are intended to handle this
type of contingency.


The files
---------

The B-tree has five associated files. DB contains the data proper of the
B-tree. The revision numbers, and other administrative information, are held in
two files, baseA and baseB. When the B-tree is opened without any particular
revision number being specified, the later of baseA and baseB is chosen as the
opening base, and as soon as a write to the file DB occurs, the earlier of
baseA or baseB is deleted. On closure, the new revision number is written to
baseB if baseA was the opening base, and to baseA if baseB was the opening
base. If the B-tree update fails for some reason, only one base will usually
survive.

Corresponding to baseA and baseB are two files bitmapA and bitmapB. Bit n is
set in the bitmap if block n is in use in the corresponding revision of the
B-tree.


The API
-------

Most of the procedures described below can return with an error condition. Error
conditions are described later. Meanwhile, the code examples given here ignore
the possibility of errors. Bad practise, but it makes them more readable.


int Btree_create(const char * name, int block_size);

    Creates a new B-tree with the given name and block size. The integer result
    gives an error condition.

        error_condition = Btree_create("/home/martin/develop/btree/", 8192);

        Btree_create("X-", 6000); /* files will be X-bitmapA, X-DB etc */

    The block size must be less than 64K, where K = 1024. It is unwise to
    use a small block size (less than 1024 perhaps), but it is not at present
    forbidden.


Thereafter there are two modes for accessing the B-tree: update and retrieval.


Update mode
-----------

struct Btree * Btree_open_to_write(const char * name);

    The name is the same as the one used in creating. The result is a pointer
    to a Btree structure, or 0 if the open fails, e.g.

        Btree * B = Btree_open_to_write("/home/martin/develop/btree/");


B->max_key_len

    This has type int and gives the upper limit of the size of a key that may
    be presented to the B-tree. As mentioned above, it will be 252 if the
    B-tree block size exceeds 1086 bytes, otherwise less. But it is of course
    good practice to use B->max_key_len, rather than writing '252' into your
    code.

    Note that for tiny B-tree block sizes this number becomes ridiculously
    small. (It is 3 when the block size is 100 bytes.)

B->revision_number

    This has type unsigned long.

    It gives the revision number of the B-tree.


    ['unsigned long' to be properly typedeffed.]

B->other_revision_number

    Similarly, this gives the revision number held in the other base file of the
    B-tree, or is zero if there is only one base file, and

B->both_bases

    is true iff both files baseA and baseB exist as valid bases.

struct Btree * Btree_open_to_write_revision(const char * name, unsigned long revision);

    Like Btree_open_to_write, but open at the given revision number.


int Btree_add(struct Btree * B, byte * key, int key_len,
                                       byte * tag, int tag_len);

    Adds the given key-tag pair to the B-tree. e.g.

        n = Btree_add(B, "TODAY", 5,
                         "Mon 9 Oct 2000", 14);

        n = Btree_add(B, k + 1, k[0], t + 1, t[0]);

    The key and tag are byte sequences addressed by 'key' and 'tag', and
    'key_len' and 'tag_len' give the number of bytes in the key and the tag.

    tag_len can be zero. key_len can be zero, and then the null item is
    replaced. If key_len exceeds the the limit on key sizes an error condition
    occurs. The result is 0 (or false) if a the key is already in the B-tree,
    otherwise 1 (or true). So the result also measures the increase in the
    total number of keys in the B-tree.


int Btree_delete(struct Btree * B, byte * key, int key_len);

    If key_len == 0 nothing happens, and the result is 0.

    Otherwise this deletes the key and its tag from the B-tree, if found. e.g.

        n = Btree_add(B, "TODAY", 5)

        n = Btree_add(B, k + 1, k[0]);

    The result is then 0 (or false) if a the key is not in the B-tree, 1 (or
    true) if it is. So the result also measures the decrease in the total
    number of keys in the B-tree.


struct Btree_item * Btree_item_create();

    Creates a structure for holding B-tree items. After

        t = Btree_item_create();

    We can make use of the following,

        t->tag_len    the length of the tag
        t->tag        pointer to the tag
        t->key_len    the length of the key
        t->key        pointer to the key

    Also accessible are

        t->tag_size
        t->key_size

    These are the number of bytes allocated to the two buffers addressed by
    t->tag and t_>key. The first time a key or tag is extracted to a Btree_item
    structure, the size of the buffer is just sufficient to accommodate it.
    Subsequently the buffer grows as larger items are placed in it.


int Btree_find_key(struct Btree * B, byte * key, int key_len);

    The result is 1 or 0 according as the looked up key is found or not found.


int Btree_find_tag(struct Btree * B, byte * key, int key_len, struct Btree_item * t);

    The same result, but when the key is found the tag is copied to t->tag and
    its length put in t->tag_len. If the key is not found t->tag and t->tag_len
    are left unchanged. e.g.

        Btree_item t = Btree_item_create();
        Btree_find_tag(B, "TODAY", 5, t); /* get today's date */


void Btree_item_lose(Btree_item * t);

    Loses the structure.


void Btree_quit(struct Btree * B);

int Btree_close(struct Btree * B, unsigned long revision);

    There are two ways of closing the B-tree. Btree_close is the normal exit,
    with all changes being flushed back to disk. Btree_quit abandons the
    update, so the revision number of the B-tree will not increase.

    The revision number of Btree_close is typically one more than the revision
    number found after opening:

        Btree * B = Btree_open_to_write("database-1/");
        unsigned long revision = B->revision_number;
        ...
        Btree_close(B, revision + 1);

    An error condition occurs if the revision number given for closing does not
    exceed the revision number of the B-tree found after opening it.

B->item_count

    This has type unsigned long.

    It returns the number of items in the B-tree, not including the
    ever-present item with null key.


Retrieval mode
--------------

In retrieval mode there is no opportunity for updating the B-tree, but access
is considerably elaborated by the use of cursors.


struct Btree * Btree_open_to_read(const char * name);

B->revision_number

struct Btree * Btree_open_to_read_revision(const char * name, unsigned long revision);

void Btree_quit(struct Btree * B);


and,

struct Btree_item * Btree_item_create();

void Btree_item_lose(struct Btree item * kt);


    These are the same as for update mode, except that that the opened B-tree
    is not modifiable.


struct Bcursor * BC = Bcursor_create(struct Btree * B);

    This creates a cursor, which can be used to remember a position inside the
    B-tree. The position is simply the item (key and tag) to which the cursor
    points. A cursor is either positioned or unpositioned, and is initially
    unpositioned.


int Bcursor_find_key(struct Bcursor * BC, byte * key, int key_len);

    The result is 1 or 0 according as the looked up key is found or not found.
    If found, the cursor is made to point to the item with the given key, and
    if not found, it is made to point to the last item in the B-tree whose key
    is <= the key being searched for, The cursor is then set as 'positioned'.
    Since the B-tree always contains a null key, which precedes everything, a
    call to Bcursor_find_key always results in a valid key being pointed to by
    the cursor.


int Bcursor_next(struct Bcursor * BC);

    If cursor BC is unpositioned, the result is simply 0.

    If cursor BC is positioned, and points to the very last item in the the
    cursor is made unpositioned, and the result is 0. Otherwise the cursor BC
    is moved to the next item in the B-tree, and the result is 1.

    Effectively, Bcursor_next(BC) loses the position of BC when it drops off
    the end of the list of items. If this is awkward, one can always arrange
    for a key to be present which has a rightmost position in a set of keys,
    e.g.

        Btree_add(B, "\xFF", 1, 0, "");
            /* all other keys have first char < xF0, and a fortiori < xFF */


int Bcursor_prev(struct Bcursor * BC);

    This is like Bcursor_next, but BC is taken to the previous rather than next
    item.


int Bcursor_get_key(struct Bcursor * BC, struct Btree_item * kt);

    If cursor BC is unpositioned, the result is simply 0.

    If BC is positioned, the key of the item is copied into kt->key and its
    length into kt->key_len. The result is then 1.

    For example,

        Bcursor * BC = Bcursor_create(B);
        Btree_item * kt = Btree_item_create();

        /* Now we'll print all the keys in the B-tree (assuming they
           have a simple form */

        Bcursor_find_key(BC, "", 0); /* must give result 1 */

        while(Btree_next(BC)) {    /* bypassing the null item */
            Bcursor_get_key(BC, kt);
            {   int i;
                for (i = 0; i < kt->key_len; i++)
                    printf("%c", kt->key[i];
                printf("\n");
            }
        }



int Bcursor_get_tag(struct Bcursor * BC, struct Btree_item * kt);

    If cursor BC is unpositioned, the result is simply 0.

    If BC is positioned, the tag of the item at cursor BC is copied into
    kt->tag and its length into kt->tag_len. BC is then moved to the next item
    as if Bcursor_next(BC) had been called - this may leave BC unpositioned.
    The result is 1 if BC is left positioned, 0 otherwise.

    For example,

        Bcursor * BC = Bcursor_create(B);
        Btree_item * kt = Btree_item_create();

        /* Now do something to each key-tag pair in the Btree */
           have a simple form */

        Bcursor_find_key(BC, "", 0); /* must give result 1 */

        while(Bcursor_get_key(BC, kt)) {
            Bcursor_get_tag(BC, kt);
            do_something_to(kt->key, kt->key_len,
                            kt->tag, kt->tag_len);
        }

        /* when BC is unpositioned by Bcursor_get_tag, Bcursor_get_key
           gives result 1 the next time it called
        */

    Tags (and keys) may be removed cheaply from a Btree_item structure kt by
    assigning kt->tag elsewhere, and the setting kt->tag to 0 and kt->tag_size
    to -1. [Perhaps there should be procedures to do this.] For example,

        Bcursor * BC = Bcursor_create(B);
        Btree_item * kt = Btree_item_create();

        unsigned char * calendar[2000];
        int year;

        for (year = 1; year <= 2000; year++) {
            unsigned char s[20]; /* 20 is fine */
            sprintf(s, "YEAR%d CALENDAR", year);
            if (Bcursor_find_key(BC, s, strlen(s))) {
                Bcursor_get_tag(BC, kt);

                calendar[year] = kt->tag;  /* may be zero */
                kt->tag = 0;
                kt->tag_size = -1;
            }
            else calendar[year] = 0;
        }
        ....

        /* later on: */

        for (year = 1; year <= 2000; year++) free(calendar[year]);


void Bcursor_lose(Bcursor * BC);

    Loses cursor BC.


Checking the B-tree
-------------------

Provisionally, the following is provided:

void Btree_check(const char * name, const char * opt_string);

    Btree_check(s, opts) is essentially equivalent to


        struct Btree * Btree_open_to_write(s);
        {
            /* do a complete integrity check of the B-tree,
               reporting according to the option string
            */
        }
        Btree_quit(B);

    The option string, if non-null, causes information to go to stdout. The
    following characters may appear in the option string:

        t   - short summary of entire B-tree
        f   - full summary of entire B-tree
        b   - print the bitmap
        v   - print the basic information (revision number, blocksize etc.)
        +   - equivalent to tbv
        ?   - lists currently available options

    The options cause a side-effect of printing, so Btree_check(s, "v") checks
    the entire B-tree and reports basic information, rather than merely
    reporting the basic information.


Full compaction
---------------

As the B-tree grows, items are added into blocks, and, when a block is full, it
splits into two (amoeba-like) and one of the new blocks accommodated the new
entry. Blocks are therefore between 50% and 100% full during growth, or 75% full
on average.

Let us say an item is 'new' if it is presented for addition to the B-tree and
its key is not already in the B-tree. Then presenting a long run of new items
ordered by key causes the B-tree updating process to switch into a mode where
much higher compaction than 75% is achieved - about 90%. This is called
'sequential' mode. It is possible to force an even higher compaction rate with
the procedure


void Btree_full_compaction(struct Btree * B, int parity);

So

    Btree_full_compaction(B, true);    /* where true == 1 */

switches full compaction on, and

    Btree_full_compaction(B, false);    /* where false == 0 */

switches it off. Full compaction may be switched on or off at any time, but
it only affects the compaction rate of sequential mode. In sequential mode, full
compaction gives around 98-99% block usage - it is not quite 100% because keys
are not split across blocks.

The downside of full compaction is that block splitting will be heavy on the
next update. However, if a B-tree is created with no intention of being updated,
full compaction is very desirable.


Full compaction with revision 1
-------------------------------

Retrieval mode is faster when the B-tree has revision number 1 than for higher
revision numbers. This is because there are no unused blocks in the B-tree and
the blocks are in a special order, and this enables the Btree_prev and
Btree_next procedures, and the other procedures which use them implicitly, to
have more efficient forms.

To make a really fast structure for retrieval therefore, create a new B-tree,
open it for updating, set full compaction mode, and add all the items in a
single transaction, sorted on keys. After closing, do not update further.
Further updates can be prevented quite easily by deleting (or moving) the bitmap
files. These are required in update mode but ignored in retrieval mode.

Here is a program fragment to unload B-tree B/ and reform it in Bnew/ as a fully
compact B-tree with revision number 1.


    {   struct Btree * B = Btree_open_to_read("B/");
        struct Bcursor * BC = Bcursor_create(B);
        struct Btree_item * item = Btree_item_create();

        Btree_create("Bnew/", 8192);

        {   struct Btree * new_B = Btree_open_to_write("Bnew/");
            Btree_full_compaction(new_B, 1);

            Bcursor_find_key(BC, "", 0);

            while(1)
            {
                if (! Bcursor_get_key(BC, item)) break;
                Bcursor_get_tag(BC, item);
                Btree_add(new_B, item->key, item->key_len,
                                 item->tag, item->tag_len);
            }
            Btree_close(new_B, 1);
        }

        Btree_item_lose(item);
        Bcursor_lose(BC);
        Btree_quit(B);
    }


Notes on space requirements
---------------------------

The level of the B-tree is the distance of the root block from a leaf block. At
minimum this is zero. If a B-tree has level L and block size B, then update
mode requires space for 2(LB + b1 + b2) bytes, where b1 and b2 are the size of
the two bitmap files. Of course, L, b1 and b2 may grow during an update on the
B-tree. If the revision number is greater than one, then retrieval mode
requires (L - 2 + 2c)B bytes, where c is the number of active cursors. If
however the revision number is one, it only requires (L - 2 + c)B bytes.

This may change in the future with code redesign, but meanwhile not that a K
term query that needs k <= K cursors open at once to process, will demand 2kB
bytes of memory in the B-tree manager.


Updating during retrieval
-------------------------

The B-tree cannot be updated by two separate processes at the same time. The
user of the B-tree software should establish a locking mechanism to ensure that
this never happens.

It is possible to do retrieval while the B-tree is being updated. If the
updating process overwrites a part of the B-tree required by the retrieval
process, the flag

    B->overwitten

is set to true. This may be detected, and suitable action taken. Here is a model
scheme:


static struct Btree * reopen(struct Btree * B)
{
    unsigned long int revision = B->revision_number;

    /* get the revision number. This will return the correct value, even when
       B->overwritten is detected during opening
    */

    Btree_quit(B);  /* close the B-tree */
    B = Btree_open_to_read(s); /* and reopen */

    if (revision == B->revision_number) {

        /* the revision number ought to have gone up from last time,
           so if we arrive here, something has gone badly wrong ...
        */
        printf("Possible database corruption - complain to Omsee\n");
        exit(1);
    }
    return B;
}


    ....

    char * s = "database/";
    struct Btree * B;
    unsigned long int revision = 0;

    B = Btree_open_to_read(s);            /* open the B-tree */

retry:

    if (B->overwritten) {
        B = reopen(s); goto retry;
    }
    ...

    Btree_item t = Btree_item_create();

    Btree_find_tag(B, "brunel", 6, t); /* look up some keyword */

    if (B->overwitten) {
        Btree_item_lose(t);
        B = reopen(s); goto retry;
    }
    ...


It may happen that B->overwitten is set to true in updating mode. This would
mean that there were two updating processes at work. If the code is correct
this does not need to be tested for, and in any case simultaneous updating is
an error that cannot generally be trapped in this way.

In retrieval mode B->overwitten should be tested after the following procedures,

    B = Btree_open_to_read(name);
    B = Btree_open_to_read_revision(name, revision);
    Bcursor_next(BC);
    Bcursor_prev(BC);
    Bcursor_find_key(BC, key, key_len);
    void Bcursor_get_tag(BC, kt);

The test is not required after any of the following,

   revision = B->revision_number;
   Btree_quit(B);
   kt = Btree_item_create();
   Btree_item_lose(kt);
   BC = Bcursor_create(B);
   Bcursor_get_key(BC, kt);
   Bcursor_lose(BC);

Note particularly that opening the B-tree can set B->overwritten, and that
Bcursor_get_key(..) will not set B->overwritten.


Error conditions
----------------

The procedures described above report errors in three ways. (A) A null result.
If after

   struct Btree_item kt = Btree_item_create();

kt == 0, the was not enough space left to create structure kt. (B) A non-zero
result. Btree_close(B) returns an int result which is 0 if successful,
otherwise an error number. (C) The error is placed in B->error, where B is the
Btree structure used in the call, or the Btree structure from which the Bcursor
structure used in the call derives. Then B->error == 0 means no error,
otherwise it is a positive number (greater than 2) giving the error number.

Some procedures cannot give an error.

Here is a summary:


    Error method  procedure
    (A)(B)(C)          error condition given by:
    ------------------------------------------------------
           *  n = Btree_find_key(B, key, key_len)
     *        kt= Btree_item_create(void)
           *  n = Btree_find_tag(B, key, key_len, kt)
                  Btree_item_lose(kt)
           *  n = Btree_add(B, key, key_len, tag, tag_len)
           *  n = Btree_delete(B, key, key_len)
     *     *  B = Btree_open_to_write(s)
     *     *  B = Btree_open_to_write_revision(s, rev)
                  Btree_quit(B)
        *     n = Btree_close(B, rev)
        *     n = Btree_create(s, block_size)
     *        B = Btree_open_to_read(s)
     *     *  B = Btree_open_to_read_revision(s, rev)
     *     *  BC= Bcursor_create(B)
           *  n = Bcursor_find_key(BC, key, key_len)
           *  n = Bcursor_next(BC)
           *  n = Bcursor_prev(BC)
           *  n = Bcursor_get_key(BC, kt)
           *  n = Bcursor_get_tag(BC, kt)
                  Bcursor_lose(BC)
                  Btree_full_compaction(B, parity)

    (A) null result (B) non-zero result (C) B->error == true

Here are some suitable error tests with the different calls,

    kt = Btree_item_create(s);
    if (kt == 0) ...

    B = Btree_open_to_write(s);
    if (B == 0 || B->error) ...

    n = Btree_create("...");
    if (n) ...

    n = Btree_delete(B, key, key_len);
    if (B->error) ...

B->error is not cleared after being set true. B->error can, as a side effect,
set B->overwritten to true, but this should not matter since the test for
B->error should always be done first. The procedures that give no error can
still have the test for error (C) applied. Error (A) will be rare, and most
of the others should be rare, except for creating and opening the Btree, which
will of course fail when the necessary files cannot be created or found.

Errors (B) and (C) have a consistent set of values, defined in btree.h. They
are,


BTREE_ERROR_BLOCKSIZE     block size too large or too small during creation
BTREE_ERROR_SPACE         malloc, calloc failure

BTREE_ERROR_BASE_CREATE   For the base files, failure to create
BTREE_ERROR_BASE_DELETE   - failure to delete
BTREE_ERROR_BASE_READ     - failure to read
BTREE_ERROR_BASE_WRITE    - failure to write

BTREE_ERROR_BITMAP_CREATE For the bit map files, failure to create
BTREE_ERROR_BITMAP_READ   - failure to read
BTREE_ERROR_BITMAP_WRITE  - failure to write

BTREE_ERROR_DB_CREATE     For the DB file, failure to create
BTREE_ERROR_DB_OPEN       - failure to open
BTREE_ERROR_DB_CLOSE      - failure to close
BTREE_ERROR_DB_READ       - failure to read
BTREE_ERROR_DB_WRITE      - failure to write

BTREE_ERROR_KEYSIZE       - key_len too large (programmer error)
BTREE_ERROR_TAGSIZE       - tag_len too large

BTREE_ERROR_REVISION      - rev too small in Btree_close (programmer error)


See 'keys and tags' above for the upper limit on tag sizes.

