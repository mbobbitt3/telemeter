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
#include "msr_safe.h"	// msr structs and ioctls
#include <stdbool.h>
#include <openssl/rand.h>
#include "keygen.h"
#include <assert.h>
#define ONE_GiB ( size_t )(1024ULL*1024*1024)
#define TELEMETRY_CPU (13)		// Run msr batches here.
#define KEY_LENGTH   (256)		// Must be a multiple of sizeof( long int ) 
#define IV_LENGTH    (128)		// Must be a multiple of sizeof( long int )
#define NUM_KEYS (10)


int
keygen(uint8_t *buf, uint16_t keylen, uint16_t hw){
	memset(buf, 0 , keylen);
    int i = 0;
    uint8_t byte_idx;
    uint8_t bit_idx;
    for(i = 0; i < hw; i++){
        while(1){
            RAND_bytes(&byte_idx, 1);
            byte_idx &= keylen - 1;
            RAND_bytes(&bit_idx, 1);
            bit_idx &= 0x07;
            if(buf[byte_idx] & (1 << bit_idx)){
                continue;
            }else{
                buf[byte_idx] |= 1 << bit_idx;
                break;
            }
        }

    }
    return 0;
}

struct crypt{
	unsigned char key[KEY_LENGTH];
	unsigned char *iv;
	unsigned char *clear_text;
	unsigned char *encrypted_text;
	EVP_CIPHER_CTX *ctx;
	uint16_t hw;
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
init_poll_energy( struct msr_batch_array *a, struct msr_batch_array *b, struct msr_batch_array *d ){
	a->numops = ONE_GiB / sizeof( struct msr_batch_op );
	a->ops = allocate_GiB_pages( 1 );

	b->numops = ONE_GiB / sizeof( struct msr_batch_op );
	b->ops = allocate_GiB_pages( 1 );

	d->numops = ONE_GiB / sizeof( struct msr_batch_op );
	d->ops = allocate_GiB_pages( 1 );
	for( uint64_t i = 0; i < a->numops; i++ ){
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
	for(uint64_t i = 0; i < b->numops; i++){
		b->ops[i].cpu		= TELEMETRY_CPU;
		b->ops[i].msrcmd	= 0xf;	// Read + A/MPERF before and after.
		b->ops[i].err		= 0;
		b->ops[i].msr		= 0x619;// DRAM_ENERGY_STATUS
		b->ops[i].msrdata	= 0;
		b->ops[i].wmask		= 0;
		b->ops[i].aperf0	= 0;
		b->ops[i].mperf0	= 0;
		b->ops[i].aperf1	= 0;
		b->ops[i].mperf1	= 0;
		b->ops[i].msrdata1	= 0;
	}
	for(uint64_t i = 0; i < d->numops; i++){
		d->ops[i].cpu		= TELEMETRY_CPU;
		d->ops[i].msrcmd	= 0x3;	// Read + A/MPERF before and after.
		d->ops[i].err		= 0;
		d->ops[i].msr		= 0x198;// PERF_STATUS
		d->ops[i].msrdata	= 0;
		d->ops[i].wmask		= 0;

	}
}

void
print_msr_data( struct msr_batch_array *a, struct msr_batch_array *b, struct msr_batch_array *d, struct crypt *c ){
	static int init = 0;
	if(!init){
		init = 1;
		fprintf( stdout, "HW cpu msrcmd err msr msrdata wmask aperf0 mperf0 aperf1 mperf1 msrdata1 cpu619 msrcmd619 err619 msr619 msrdata619 619aperf0 619mperf0 619aperf1 619mperf1 619msrdata1 cpu198 msrcmd198 err198 msr198 msrdata198 \n" );
	}
	for( uint64_t i=0; i<a->numops; i++ ){
		fprintf( stdout, 
			//hw	//cpu        msrcmd        err       msr           msrdata        wmask
			"%02"PRIu16 " %02"PRIu16" 0x%04"PRIx16" %"PRId32" 0x%08"PRIx32" %lf"" 0x%016"PRIx64
			//aperf0         mperf0         aperf1         mperf1         msrdata1
			" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" %lf"
		    //cpu619     //msrcmd619    //err619  //msr619     //msrdata619   
			" %02"PRIu16" 0x%04"PRIx16" %"PRId32" 0x%08"PRIx32" %lf"		
			//aperf0         mperf0         aperf1         mperf1         msrdata1
			" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" %lf"
		  	//cpu198     //msrcmd198    //err198  //msr198     //msrdata198   
			" %02"PRIu16" 0x%04"PRIx16" %"PRId32" 0x%08"PRIx32" 0x%016"PRIx64	
		  
			"\n",
			(int)(c->hw),
			(uint16_t)(a->ops[i].cpu),
			(uint16_t)(a->ops[i].msrcmd),
			( int32_t)(a->ops[i].err),
			(uint32_t)(a->ops[i].msr),
			(double)(a->ops[i].msrdata) / (1<<14),
			(uint64_t)(a->ops[i].wmask),
			(uint64_t)(a->ops[i].aperf0),
			(uint64_t)(a->ops[i].mperf0),
			(uint64_t)(a->ops[i].aperf1),
			(uint64_t)(a->ops[i].mperf1),
			(double)(a->ops[i].msrdata1) / (1<<14),
		
			(uint16_t)(b->ops[i].cpu),
			(uint16_t)(b->ops[i].msrcmd),
			( int32_t)(b->ops[i].err),
			(uint32_t)(b->ops[i].msr),
			(double)(b->ops[i].msrdata) / (1<<14),
			(uint64_t)(b->ops[i].aperf0),
			(uint64_t)(b->ops[i].mperf0),
			(uint64_t)(b->ops[i].aperf1),
			(uint64_t)(b->ops[i].mperf1),
			(double)(b->ops[i].msrdata1) / (1<<14),

			(uint16_t)(d->ops[i].cpu),
			(uint16_t)(d->ops[i].msrcmd),
			( int32_t)(d->ops[i].err),
			(uint32_t)(d->ops[i].msr),
			(uint64_t)(d->ops[i].msrdata)
		
		);
	}	
}

void
batch_ioctl( struct msr_batch_array *a, struct msr_batch_array *b, struct msr_batch_array *d ){
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

	if( NULL == b ){
		close( batch_fd );
		Initialized = 0;
		return;
	}

	if( NULL == d ){
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

	int rc_b = ioctl( batch_fd, X86_IOC_MSR_BATCH, b );
	if( -1 == rc_b ){
		fprintf( stderr, "%s:%d ioctl on /dev/cpu/msr_batch failed, errno=%d.\n",
				__FILE__, __LINE__, errno );
		perror( "perror() reports: " );
		exit( -1 );
	}

	int rc_d  = ioctl( batch_fd, X86_IOC_MSR_BATCH, d );
	if( -1 == rc_d ){
		fprintf( stderr, "%s:%d ioctl on /dev/cpu/msr_batch failed, errno=%d.\n",
				__FILE__, __LINE__, errno );
		perror( "perror() reports: " );
		exit( -1 );
	}
}

void
telemeter_init( struct msr_batch_array *a, struct msr_batch_array *b, struct msr_batch_array *d ){
	init_poll_energy( a, b, d );
	a->numops=5000;
	b->numops = 5000;
	d->numops = 5000;	// 1k per second, max is ONE_GiB/sizeof(msr_batch_array) ~ 15M.
}

double
telemeter( struct msr_batch_array *a, struct msr_batch_array *b, struct msr_batch_array *d ){
	struct timeval start, stop;
	gettimeofday( &start, NULL );
	batch_ioctl( a, b, d );
	gettimeofday( &stop, NULL );
	return (stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec)/1000000.0;
}

void
telemeter_finalize(){
	batch_ioctl( NULL, NULL, NULL  );
}

void
payload_init( struct crypt *c ){
	// Allocate 1GiB for cleartext and encrypted text.
	static unsigned char *cleartext;
	static unsigned char *encrypted_text;
	static int init = 0;
	static long int iv[IV_LENGTH/sizeof(long int)];
	if(!init){
		init = 1;
		cleartext = allocate_GiB_pages( 1 );	
		encrypted_text = allocate_GiB_pages( 2 );
		// Initialize the random number generator.
		srandom( 13 );
	
		// Randomize the cleartext buffer.
		for( uint64_t i=0; i<(ONE_GiB/sizeof(long int)); i++ ){
			((long int*)(cleartext))[i] = random();
		}

		// Set the initialization vector to a random number.
		for( uint64_t i=0; i<(IV_LENGTH/sizeof(long int)); i++ ){
			iv[i] = random();
		}
		// Zero out the encrypted text buffer.
		memset(encrypted_text, 0, ONE_GiB );	// Necessary?
	}
	c->clear_text = cleartext; 
	c->encrypted_text = encrypted_text;
	c->iv = (unsigned char *) iv;
	keygen(c->key, KEY_LENGTH, c->hw);
	
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
	gettimeofday( &start, NULL );
	int rc = EVP_EncryptUpdate(
			c->ctx,			// EVP_CIPHER_CTX*
			c->encrypted_text,	// unsigned char *out
			&(c->out_len),		// int *outl
			c->clear_text,		// const unsigned char *in
			(int)ONE_GiB);		// int inl
	if( !rc ){
		fprintf( stderr, "%s:%d EVP_EncryptUpdate() failed, returned 0.\n", __FILE__, __LINE__ );
		exit( -1 );
	}
	gettimeofday( &stop, NULL );
	return (stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec)/1000000.0;
}

void
payload_finalize(struct crypt *c){
	EVP_CIPHER_CTX_free( c->ctx );
}

int main(){
	
	int tid;
	struct msr_batch_array a;
	struct msr_batch_array b;
	struct msr_batch_array d;
	struct crypt c[NUM_KEYS];
	double elapsed_keys[NUM_KEYS];
	double elapsed_msr[1];
	int key_id = 0;
	telemeter_init( &a, &b, &d );
	for( key_id = 0; key_id < NUM_KEYS; key_id++){
		c[key_id].hw = key_id * 100;	
		payload_init( &c[key_id]);
	}

	omp_set_num_threads(2);

	for(key_id = 0; key_id < NUM_KEYS; key_id++){  		
		#pragma omp parallel num_threads(2)
		{
			tid = omp_get_thread_num();
			if( 0 == tid ){
				elapsed_msr[0] = telemeter( &a, &b, &d );
			}else{
				elapsed_keys[key_id] = payload( &c[key_id] );
			}
		}

		fprintf( stderr, "%s:%d Telemeter elapsed seconds:  %lf\n", __FILE__, __LINE__, elapsed_msr[0] );
		fprintf( stderr, "%s:%d Payload elapsed seconds:    %lf\n", __FILE__, __LINE__, elapsed_keys[key_id] );
		print_msr_data( &a, &b, &d, &c[key_id] );
		payload_finalize( &c[key_id] );
	}


	telemeter_finalize();

	return 0;
}
