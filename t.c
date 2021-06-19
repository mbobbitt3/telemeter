/*******************************************************************************
 * t.c
 *
 * author:  Barry Rountree, rountree@llnl.gov
 * This software has not yet been reviewed or released for distribution.
 ******************************************************************************/

#include <stdio.h>			// fprintf(), perror()
#include <hugetlbfs.h>			// get_huge_pages()
#include <stdint.h>			// uint8_t and friends
#include <inttypes.h>			// PRIx8 and friends
#include <errno.h>			// definition of errno
#include <stdlib.h>			// exit()
#include <stdbool.h>			// bool
#include <sys/types.h>			// open()
#include <sys/stat.h>			// open()
#include <fcntl.h>			// open()
#include <sys/ioctl.h>			// ioctl()
#include <unistd.h>			// close()
#include "../msr-safe/msr_safe.h"	// msr structs and ioctls


#define ONE_GiB ( size_t )(1024ULL*1024*1024)
#define TELEMETRY_CPU (13)		// Run msr batches here.

void*
allocate_GiB_pages( size_t n_GiB ){
	long sz = gethugepagesize();
	if( -1 == sz ){
		fprintf(stderr, "%s:%d gethugepagesize() failed (errno=%d)",
			       	__FILE__, __LINE__, errno); 
		perror("perror reports:  ");
		exit( -1 );
	}
	if( sz != ONE_GiB ){
		fprintf(stderr, "%s:%d gethugepagesize() reports the default huge page size is %ld.\n", __FILE__, __LINE__, sz);
		fprintf(stderr, "We only support %zu huge pages at the moment.", ONE_GiB );
		exit( -1 );
	}

	uint8_t *op = get_huge_pages( n_GiB * ONE_GiB, GHP_DEFAULT );
	if( NULL == op ){
		fprintf(stderr, "%s:%d get_huge_pages() failed (errno=%d)",
			       	__FILE__, __LINE__, errno); 
		perror("perror reports:  ");
		exit( -1 );
	}
	return op;
}

void
free_GiB_pages( void *p ){
	free_huge_pages( p );
}

void
init_poll_energy( struct msr_batch_array *a ){
	a->numops = ONE_GiB / sizeof( struct msr_batch_op );
	a->ops = allocate_GiB_pages( 1 );
	for( uint64_t i=0; i<a->numops; i++ ){
		a->ops[i].cpu		= TELEMETRY_CPU;
		a->ops[i].msrcmd	= 0xf;	// Read + A/MPERF before and after.
		a->ops[i].err		= 0;
		a->ops[i].msr		= 0x611;// PKG_ENERGY_STATUS
		a->ops[i].msrdata	= 0;
		a->ops[i].wmask		= 0;
		a->ops[i].aperf0	= 0;
		a->ops[i].mperf0	= 0;
		a->ops[i].aperf1	= 0;
		a->ops[i].mperf1	= 0;
		a->ops[i].msrdata1	= 0;
	}
}

void
print_msr_data( struct msr_batch_array *a ){
	fprintf( stdout, "cpu msrcmd err msr msrdata wmask aperf0 mperf0 aperf1 mperf1 msrdata1\n" );
	for( uint64_t i=0; i<a->numops; i++ ){
		fprintf( stdout, 
			//cpu        msrcmd        err       msr           msrdata        wmask
			"%02"PRIu16" 0x04%"PRIx16" %"PRId32" 0x08%"PRIx32" 0x016%"PRIx64" 0x%016"PRIx64
			//aperf0         mperf0         aperf1         mperf1         msrdata1
			" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIu64"\n",
			(uint16_t)(a->ops[i].cpu),
			(uint16_t)(a->ops[i].msrcmd),
			( int32_t)(a->ops[i].err),
			(uint32_t)(a->ops[i].msr),
			(uint64_t)(a->ops[i].msrdata),
			(uint64_t)(a->ops[i].wmask),
			(uint64_t)(a->ops[i].aperf0),
			(uint64_t)(a->ops[i].mperf0),
			(uint64_t)(a->ops[i].aperf1),
			(uint64_t)(a->ops[i].mperf1),
			(uint64_t)(a->ops[i].msrdata1)
		);
	}	
}

void
batch_ioctl( struct msr_batch_array *a ){
	static bool Initialized=0;
	static int batch_fd;
	if( !Initialized ){
		batch_fd = open( "/dev/cpu/msr_batch", O_RDWR );
		if ( -1 == batch_fd ){
			fprintf( stderr, "%s:%d open() failed on /dev/cpu/msr_batch, errno=%d.\n",
					__FILE__, __LINE__, errno );
			perror( "perror() reports: " );
			exit( -1 );
		}
	}
	if( NULL == a ){
		close( batch_fd );
		return;
	}
	int rc = ioctl( batch_fd, X86_IOC_MSR_BATCH, a );
	if( -1 == rc ){
		fprintf( stderr, "%s:%d ioctl on /dev/cpu/msr_batch failed, errno=%d.\n",
				__FILE__, __LINE__, errno );
		perror( "perror() reports: " );
		exit( -1 );
	}
}

int main(){
	struct msr_batch_array a;
	init_poll_energy( &a );
	batch_ioctl( &a );
	print_msr_data( &a );
	batch_ioctl( NULL );
	return 0;
}
