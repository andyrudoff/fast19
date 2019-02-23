/*
 * freq_pmem_cpp.cpp -- pmem-based word frequency counter, C++ version
 *
 * create the pool for this program using pmempool, for example:
 *	pmempool create obj --layout=freq -s 1G freqcount
 *	freq_pmem_cpp freqcount file1.txt file2.txt...
 */
#include <cctype>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <iostream>
#include <thread>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/pext.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>
#include <libpmemobj++/mutex.hpp>

#define LAYOUT "freq"
#define NBUCKETS 10007

using pmem::obj::p;
using pmem::obj::persistent_ptr;
using pmem::obj::pool;
using pmem::obj::pool_base;
using pmem::obj::make_persistent;
using pmem::obj::delete_persistent;
using pmem::obj::transaction;
using pmem::obj::mutex;

/* entries in a bucket are a linked list of struct entry */
struct entry {

	entry(int ct, const char *wrd,
		const persistent_ptr<struct entry> &nxt) :
		next{nxt},
		word{pmemobj_tx_strdup(wrd, pmem::detail::type_num<char>())},
		count{ct}
	{}

	persistent_ptr<struct entry> next;
	persistent_ptr<char> word;
	mutex mtx;		/* protects count field */
	p<int> count;
};

/* each bucket contains a pointer to the linked list of entries */
struct bucket {
	mutex mtx;		/* protects entries field */
	persistent_ptr<struct entry> entries;
};

using buckets = persistent_ptr<bucket[NBUCKETS]>;

class freq {
	/* hash a string into an index into h[] */
	unsigned hash(const char *s)
	{
		unsigned h = NBUCKETS ^ ((unsigned)*s++ << 2);
		unsigned len = 0;

		while (*s) {
			len++;
			h ^= (((unsigned)*s) << (len % 3)) +
			    ((unsigned)*(s - 1) << ((len % 3 + 7)));
			s++;
		}
		h ^= len;

		return h % NBUCKETS;
	}

	/* bump the count for a word */
	void count(const char *word)
	{
		unsigned h = hash(word);

		auto &mtx = ht[h].mtx;
		mtx.lock();

		auto ep = ht[h].entries;

		for (; ep != nullptr; ep = ep->next)
			if (strcmp(word, ep->word.get()) == 0) {
				/* already in table, just bump the count */

				/* drop bucket lock */
				mtx.unlock();

				/* lock entry and update it transactionally */
				transaction::run(pop, [&ep]() {

					ep->count++;

				}, ep->mtx);

				return;
			}

		/* allocate new entry in table */
		transaction::run(pop, [&ep, &word, this, &h]() {

			/* allocate new entry */
			ep = make_persistent<entry>(1, word, ht[h].entries);

			/* add it to the front of the linked list */
			ht[h].entries = ep;
		});

		mtx.unlock();
	}



public:
	freq(buckets bht, pool_base &pool) : ht{bht}, pop{pool}
	{}

	/* break a test file into words and call count() on each one */
	void count_all_words(const char *fname)
	{
		FILE *fp;
		int c;
		char word[MAXWORD];
		char *ptr;

		if ((fp = fopen(fname, "r")) == NULL)
			throw std::runtime_error(std::string("fopen: ") +
									fname);
		ptr = NULL;
		while ((c = getc(fp)) != EOF)
			if (isalpha(c)) {
				if (ptr == NULL) {
					/* starting a new word */
					ptr = word;
					*ptr++ = c;
				} else if (ptr < &word[MAXWORD - 1])
					/* add character to current word */
					*ptr++ = c;
				else {
					/* word too long, truncate it */
					*ptr++ = '\0';
					count(word);
					ptr = NULL;
				}
			} else if (ptr != NULL) {
				/* word ended, store it */
				*ptr++ = '\0';
				count(word);
				ptr = NULL;
			}

		/* handle the last word */
		if (ptr != NULL) {
			/* word ended, store it */
			*ptr++ = '\0';
			count(word);
		}

		fclose(fp);
	}

private:
	buckets ht;	/* pointer to the buckets */
	pool_base &pop;
	static const int MAXWORD = 8192;
};

struct root {
	buckets ht;	/* word frequencies */
	/* ... other things we store in this pool go here... */
};

int main(int argc, char *argv[])
{
	int arg = 2;	/* index into argv[] for first file name */

	if (argc < 3) {
		std::cerr << "usage: " << argv[0]
			  << " pmemfile wordfiles..." << std::endl;
		exit(1);
	}

	auto pop = pool<root>::open(argv[1], LAYOUT);
	auto q = pop.root();

	/* before starting, see if buckets have been allocated */
	if (q->ht == nullptr) {
		transaction::run(pop, [&q]() {
			q->ht = make_persistent<bucket[NBUCKETS]>();
		});
	}

	int nfiles = argc - arg;
	std::vector<std::thread> threads;

	for (int i = 0; i < nfiles; ++i)
		threads.emplace_back(&freq::count_all_words, freq(q->ht, pop),
				argv[arg++]);

	for (auto &t : threads)
		t.join();

	pop.close();
	exit(0);
}
