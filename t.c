/*******************************************************************************
 * t.c
 *
 * author:  Barry Rountree, rountree@llnl.gov
 * This software has not yet been reviewed or released for distribution.
 ******************************************************************************/

#include <stdio.h>		// fprintf(), perror()
#include <hugetlbfs.h>		// get_huge_pages()
#include <stdint.h>		// uint8_t and friends
#include <inttypes.h>		// PRIx8 and friends
#include <errno.h>		// definition of errno
#include <stdlib.h>		// exit()
#include "../msr-safe/msr_safe.h"


#define ONE_GiB (1024ULL*1024*1024)

void*
allocate_GiB_pages( size_t n_GiB ){
	long sz = gethugepagesize();
	if( -1 == sz ){
		fprintf(stderr, "%s:%d gethugepagesize() failed (errno=%d)",
			       	__FILE__, __LINE__, errno); 
		perror("perror reports:  ");
		exit(-1);
	}
	if( sz != ONE_GiB ){
		fprintf(stderr, "%s:%d gethugepagesize() reports the default huge page size is %ld.\n", __FILE__, __LINE__, sz);
		fprintf(stderr, "We only support %ld huge pages at the moment.", ONE_GiB );
		exit(-1);
	}

	uint8_t *op = get_huge_pages( n_GiB * ONE_GIGABYTE, GHP_DEFAULT );
	if( NULL == op ){
		fprintf(stderr, "%s:%d get_huge_pages() failed (errno=%d)",
			       	__FILE__, __LINE__, errno); 
		perror("perror reports:  ");
		exit(-1);
	}
	return op;
}

void
free_GiB_pages( void *p ){
	free_huge_pages( p );
}

void
init_poll_energy( struct msr_batch_array *a ){

}

void
poll_energy(){

}

void
finalize_poll_energy(){

}

void
dump_poll_energy(){

}


int main(){

	return 0;
}
