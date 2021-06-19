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

#define ONE_GIGABYTE (1024ULL*1024*1024*1)

int main(){
	fprintf( stdout, "gethugepagesize() returns %ld.\n", gethugepagesize() );
	uint8_t *op = get_huge_pages( 2 * ONE_GIGABYTE, GHP_DEFAULT );
	if( NULL == op ){
		fprintf(stderr, "%s:%d get_huge_pages failed (errno=%d)",
			       	__FILE__, __LINE__, errno); 
		perror("perror reports:  ");
		exit(-1);
	}
	free_huge_pages(op);
	return 0;
}
