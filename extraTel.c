/*******************************************************************************
 * t.c
 *
 * author:  Barry Rountree, rountree@llnl.gov
 * This software has not yet been reviewed or released for distribution.
 ******************************************************************************/

#define _DEFAULT_SOURCE			// required for random(), srandom()
#include <stdlib.h>			// exit(), random(), srandom()
#include <string.h>			// memset()
#include <stdio.h>			// fprintf(), perror()
#include <hugetlbfs.h>			// get_huge_pages()
#include <stdint.h>			// uint8_t and friends
#include <inttypes.h>			// PRIx8 and friends
#include <errno.h>			// definition of errno
#include <stdbool.h>			// bool
#include <sys/types.h>			// open()
#include <sys/stat.h>			// open()
#include <fcntl.h>			// open()
#include <sys/ioctl.h>			// ioctl()
#include <unistd.h>			// close()
#include <omp.h>			// omp_get_thread_id()
#include <openssl/evp.h>		// EVP_CIPHER_CTX_new(), EVP_EncryptInit_ex
#include <sys/time.h>			// gettimeofday()
#include <assert.h>
#include "msr_safe.h"			// msr structs and ioctls

#define ONE_GiB ( size_t )(1024ULL*1024*1024)
#define TELEMETRY_CPU (13)		// Run msr batches here.
#define TELEMETRY_CPU_DRAM (14)		// Run msr batches here.
#define TELEMETRY_CPU_PERF_STATUS (15)		// Run msr batches here.
#define KEY_LENGTH   (256)		// Must be a multiple of sizeof( long int )
#define IV_LENGTH    (128)		// Must be a multiple of sizeof( long int )

struct crypt{
	unsigned char key[256];
	unsigned char iv[128];
	unsigned char *clear_text;
	unsigned char *encrypted_text;
	EVP_CIPHER_CTX *ctx;
	int out_len;
	int in_len;
};

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
	a[0].numops = ONE_GiB / sizeof( struct msr_batch_op );
	a[0].ops = allocate_GiB_pages( 1 );
	for( uint64_t i=0; i<a[0].numops; i++ ){
		a[0].ops[i].cpu			= TELEMETRY_CPU;
		a[0].ops[i].msrcmd		= 0xf;	// Read + A/MPERF before and after.
		a[0].ops[i].err			= 0;
		a[0].ops[i].msr			= 0x611;// PKG_ENERGY_STATUS
		a[0].ops[i].msrdata		= 0;
		a[0].ops[i].wmask		= 0;
		a[0].ops[i].mperf0		= 0;
		a[0].ops[i].mperf1		= 0;
		a[0].ops[i].msrdata1		= 0;
	}

	a[1].numops = ONE_GiB / sizeof( struct msr_batch_op);
	a[1].ops = allocate_GiB_pages( 1 );
	for( uint64_t i=0; i<a[1].numops; i++ ){
		a[1].ops[i].cpu			= TELEMETRY_CPU_DRAM;
		a[1].ops[i].msrcmd		= 0xf;	// Read + A/MPERF before and after.
		a[1].ops[i].err			= 0;
		a[1].ops[i].msr			= 0x619;// DRAM_ENERGY_STATUS
		a[1].ops[i].msrdata		= 0;
		a[1].ops[i].wmask		= 0;
		a[1].ops[i].mperf0		= 0;
		a[1].ops[i].mperf1		= 0;
		a[1].ops[i].msrdata1		= 0;
	}

	a[2].numops = ONE_GiB / sizeof( struct msr_batch_op);
	a[2].ops = allocate_GiB_pages( 1 );
	for( uint64_t i=0; i<a[2].numops; i++ ){
		a[2].ops[i].cpu			= TELEMETRY_CPU_PERF_STATUS;
		a[2].ops[i].msrcmd		= 0xf;	// Read + A/MPERF before and after.
		a[2].ops[i].err			= 0;
		a[2].ops[i].msr			= 0x198;// MSR_PERF_STATUS
		a[2].ops[i].msrdata		= 0;
		a[2].ops[i].wmask		= 0;
		a[2].ops[i].mperf0		= 0;
		a[2].ops[i].mperf1		= 0;
		a[2].ops[i].msrdata1		= 0;
	}
}

void
print_msr_data( struct msr_batch_array *a ){
	fprintf( stdout, "cpu_pkg msrcmd_pkg err_pkg msr_pkg start_pkg_msrdata wmask_pkg mperf_pkg0 mperf_pkg1 end_pkg_msrdata cpu_dram msrcmd_dram err_dram msr_dram start_dram_msrdata wmask_dram mperf_dram0 mperf_dram1 end_dram_msrdata cpu_perf_status msrcmd_perf err_perf msr_perf start_perf_msrdata wmask_perf mperf_perf0 mperf_perf1 end_perf_msrdata\n" );
	for( uint64_t i=0; i<a[0].numops; i++ ){
		fprintf( stdout, 
			//cpu        msrcmd        err       msr           msrdata        wmask
			"%02"PRIu16" 0x%04"PRIx16" %"PRId32" 0x%08"PRIx32" %lf" " 0x%016"PRIx64
			//mperf0         mperf1        msrdata1
			" 0x%016"PRIx64" 0x%016"PRIx64" %lf" 
			//cpu619     msrcmd619      err619       msr619           msrdata619        wmask619
                         " %02"PRIu16" 0x%04"PRIx16" %"PRId32" 0x%08"PRIx32" %lf" " 0x%016"PRIx64
                         //mperf0619      mperf1619      msrdata1619
                         " 0x%016"PRIx64" 0x%016"PRIx64" %lf" 
			//cpu198        msrcmd198        err198       msr198           msrdata198        wmask198
                         " %02"PRIu16" 0x%04"PRIx16" %"PRId32" 0x%08"PRIx32" 0x%016"PRIx64" 0x%016"PRIx64
                         //mperf0198      mperf1198      msrdata1198
                         " 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64"\n",
			(uint16_t)(a[0].ops[i].cpu),
			(uint16_t)(a[0].ops[i].msrcmd),
			( int32_t)(a[0].ops[i].err),
			(uint32_t)(a[0].ops[i].msr),
			/*(uint64_t)*/(double)(a[0].ops[i].msrdata)/(1 << 14),
			(uint64_t)(a[0].ops[i].wmask),
			(uint64_t)(a[0].ops[i].mperf0),
			(uint64_t)(a[0].ops[i].mperf1),
			/*(uint64_t)*/(double)(a[0].ops[i].msrdata1)/(1 << 14),

			(uint16_t)(a[1].ops[i].cpu),
                        (uint16_t)(a[1].ops[i].msrcmd),
                        ( int32_t)(a[1].ops[i].err),
                        (uint32_t)(a[1].ops[i].msr),
                        /*(uint64_t)*/(double)(a[1].ops[i].msrdata)/(1 << 14),
                        (uint64_t)(a[1].ops[i].wmask),
                        (uint64_t)(a[1].ops[i].mperf0),
                        (uint64_t)(a[1].ops[i].mperf1),
                        /*(uint64_t)*/(double)(a[1].ops[i].msrdata1)/(1 << 14),

			(uint16_t)(a[2].ops[i].cpu),
                        (uint16_t)(a[2].ops[i].msrcmd),
                        ( int32_t)(a[2].ops[i].err),
                        (uint32_t)(a[2].ops[i].msr),
                        (uint64_t)(a[2].ops[i].msrdata),
                        (uint64_t)(a[2].ops[i].wmask),
                        (uint64_t)(a[2].ops[i].mperf0),
                        (uint64_t)(a[2].ops[i].mperf1),
                        (uint64_t)(a[2].ops[i].msrdata1)

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
		Initialized = 1;
	}
	if( NULL == a ){
		close( batch_fd );
		Initialized = 0;
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

void
telemeter_init( struct msr_batch_array *a ){
	init_poll_energy( a );
	a[0].numops=5000;		// 1k per second, max is ONE_GiB/sizeof(msr_batch_array) ~ 15M.
	a[1].numops=5000;		// 1k per second, max is ONE_GiB/sizeof(msr_batch_array) ~ 15M.
	a[2].numops=5000;		// 1k per second, max is ONE_GiB/sizeof(msr_batch_array) ~ 15M.
}

double
telemeter( struct msr_batch_array *a ){
	struct timeval start, stop;
	gettimeofday( &start, NULL );
	batch_ioctl( a );
	gettimeofday( &stop, NULL );
	return (stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec)/1000000.0;
}

void
telemeter_finalize(){
	batch_ioctl( NULL );
}

void
payload_init( struct crypt *c ){
	// Allocate 1GiB for cleartext and encrypted text.
	c->clear_text = allocate_GiB_pages( 1 );	
	c->encrypted_text = allocate_GiB_pages( 2 );	// Massive overkill.

	// Initialize the random number generator.
	srandom( 13 );
	
	// Randomize the cleartext buffer.
	/*
	for( uint64_t i=0; i<(ONE_GiB/sizeof(long int)); i++ ){
		((long int*)(c->clear_text))[i] = random();
	}
	*/

	// Zero out the encrypted text buffer.
	// memset( c->encrypted_text, 0, ONE_GiB );	// Necessary?

	// Set the key to a random number
	for( uint64_t i=0; i<(KEY_LENGTH/sizeof(long int)); i++ ){
		((long int*)(c->key))[i] = random();
	}

	// Set the initialization vector to a random number.
	for( uint64_t i=0; i<(IV_LENGTH/sizeof(long int)); i++ ){
		((long int*)(c->iv))[i] = random();
	}

	// Get a new context.
	c->ctx = EVP_CIPHER_CTX_new();
	if( !(c->ctx) ){
		fprintf( stderr, "%s:%d EVP_CIPHER_CTX_new() failed, returned NULL.\n", __FILE__, __LINE__ );
		exit( -1 );
	}

	// Initialize the encryption algorithm.
	int rc = EVP_EncryptInit_ex( 
			c->ctx, 		// EVP_CIPHER_CTX*
			EVP_aes_256_cbc(),	// EVP_CIPHER*
			NULL,			// ENGINE*
			c->key,			// const unsigned char*
			c->iv);			// const unsigned char*
	if( !rc ){
		fprintf( stderr, "%s:%d EVP_EncryptInit_ex() failed, returned 0.\n", __FILE__, __LINE__ );
		exit( -1 );
	}

	return;	
}


double
payload(struct crypt *c){
	struct timeval start, stop;
	c->key[2] = 0xff;
	gettimeofday( &start, NULL );
	for( uint64_t i=0; i<80000001ULL; i++ ){
		// Get a new context.
		c->ctx = EVP_CIPHER_CTX_new();
		if( !(c->ctx) ){
			fprintf( stderr, "%s:%d EVP_CIPHER_CTX_new() failed, returned NULL.\n", __FILE__, __LINE__ );
			exit( -1 );
		}

		// Initialize the encryption algorithm.
		int rc = EVP_EncryptInit_ex( 
				c->ctx, 		// EVP_CIPHER_CTX*
				EVP_aes_256_cbc(),	// EVP_CIPHER*
				NULL,			// ENGINE*
				c->key,			// const unsigned char*
				c->iv);			// const unsigned char*
		if( !rc ){
			fprintf( stderr, "%s:%d EVP_EncryptInit_ex() failed, returned 0.\n", __FILE__, __LINE__ );
			exit( -1 );
		}

		rc = EVP_EncryptUpdate(
				c->ctx,			// EVP_CIPHER_CTX*
				c->encrypted_text,	// unsigned char *out
				&(c->out_len),		// int *outl
				c->clear_text,		// const unsigned char *in
				(int)16);		// int inl
		if( !rc ){
			fprintf( stderr, "%s:%d EVP_EncryptUpdate() failed, returned 0.\n", __FILE__, __LINE__ );
			exit( -1 );
		}
		EVP_CIPHER_CTX_free( c->ctx );
	}
	gettimeofday( &stop, NULL );
	return (stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec)/1000000.0;
}

void
payload_finalize(struct crypt *c){
	//EVP_CIPHER_CTX_free( c->ctx );
}



int main(){
	int tid;
	struct msr_batch_array a[5] = {0};
	struct crypt c;
	double elapsed[4];

	telemeter_init( a );
	payload_init( &c );

	c.key[1] = 0x11;

	omp_set_num_threads(4);
#pragma omp parallel shared(elapsed) num_threads(4)
	{
		tid = omp_get_thread_num();
		switch( tid ){
			case 0: elapsed[0] = telemeter( &(a[0]) ); 	break;
			case 1: elapsed[1] = telemeter( &(a[1]) ); 	break;
			case 2: elapsed[2] = telemeter( &(a[2]) ); 	break;
			case 3: elapsed[3] = payload( &c );	   	break;
			default: assert(0); 				break;
		}
		/*
		if( 0 )!= tid ){
			elapsed[0] = telemeter( &(a[0]) );
			elapsed[1] = telemeter( &(a[1]) );
			elapsed[2] = telemeter( &(a[2]) );
		}else{
			elapsed[3] = payload( &c );
		}
		*/
	}
	fprintf( stderr, "%s:%d Telemeter elapsed seconds:  %lf\n", __FILE__, __LINE__, elapsed[0] );
	fprintf( stderr, "%s:%d Payload elapsed seconds:    %lf\n", __FILE__, __LINE__, elapsed[1] );

	telemeter_finalize();
	payload_finalize( &c );

	print_msr_data( a );

	return 0;
}
