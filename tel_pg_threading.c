/*******************************************************************************
 * t.c
 *
 * author:  Barry Rountree, rountree@llnl.gov
 * This software has not yet been reviewed or released for distribution.
 ******************************************************************************/
#define _GNU_SOURCE // sched_getcpu(3) is glibc-specific (see the man page)
#define _DEFAULT_SOURCE			// required for random(), srandom()
#include <stdlib.h>			// exit(), random(), srandom()
#include <string.h>			// memset()
#include <stdio.h>			// fprintf(), perror()
#include <sched.h>
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
#include <openssl/err.h>
#include "keygen.h"
#include <assert.h>
#define ONE_GiB ( size_t )(1024ULL*1024*1024)
#define TELEMETRY_CPU (13)		// Run msr batches here.
#define KEY_LENGTH   (256)		// Must be a multiple of sizeof( long int ) 
#define IV_LENGTH    (128)		// Must be a multiple of sizeof( long int )
#define NUM_KEYS (10)
#define NUM_RUNS (10)
void handleErrors(void)
{
  ERR_print_errors_fp(stderr);
  abort();
}

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
//	unsigned char *iv;
	unsigned char *clear_text;
	unsigned char *encrypted_text;
	EVP_CIPHER_CTX *ctx;
	uint8_t run;
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
init_poll_energy( struct msr_batch_array *a){
	a[0].numops = ONE_GiB / sizeof( struct msr_batch_op );
	a[0].ops = allocate_GiB_pages( 1 );

	a[1].numops = ONE_GiB / sizeof( struct msr_batch_op );
	a[1].ops = allocate_GiB_pages( 1 );

	a[2].numops = ONE_GiB / sizeof( struct msr_batch_op );
	a[2].ops = allocate_GiB_pages( 1 );
	for( uint64_t i = 0; i < a[0].numops; i++ ){
		a[0].ops[i].cpu			= TELEMETRY_CPU;
		a[0].ops[i].msrcmd		= 0xf;	// Read + A/MPERF before and after.
		a[0].ops[i].err			= 0;
		a[0].ops[i].msr			= 0x611;// PKG_ENERGY_STATUS
		a[0].ops[i].msrdata		= 0;
		a[0].ops[i].wmask		= 0;
		a[0].ops[i].aperf0		= 0;
		a[0].ops[i].mperf0		= 0;
		a[0].ops[i].aperf1		= 0;
		a[0].ops[i].mperf1		= 0;
		a[0].ops[i].msrdata1	= 0;
	}
	for(uint64_t i = 0; i < a[1].numops; i++){
		a[1].ops[i].cpu		= TELEMETRY_CPU + 1;
		a[1].ops[i].msrcmd		= 0xf;	// Read + A/MPERF before and after.
		a[1].ops[i].err			= 0;
		a[1].ops[i].msr			= 0x619;// DRAM_ENERGY_STATUS
		a[1].ops[i].msrdata		= 0;
		a[1].ops[i].wmask		= 0;
		a[1].ops[i].aperf0		= 0;
		a[1].ops[i].mperf0		= 0;
		a[1].ops[i].aperf1		= 0;
		a[1].ops[i].mperf1		= 0;
		a[1].ops[i].msrdata1	= 0;
	}
	for(uint64_t i = 0; i < a[2].numops; i++){
		a[2].ops[i].cpu			= TELEMETRY_CPU + 2;
		a[2].ops[i].msrcmd		= 0xf;	// Read + A/MPERF before and after.
		a[2].ops[i].err			= 0;
		a[2].ops[i].msr			= 0x1b1;// THERM_PACKAGE_STATUS
		a[2].ops[i].msrdata		= 0;
		a[2].ops[i].wmask		= 0;
		a[2].ops[i].aperf0		= 0;
		a[2].ops[i].mperf0		= 0;
		a[2].ops[i].aperf1		= 0;
		a[2].ops[i].mperf1		= 0;
		a[2].ops[i].msrdata1	= 0;
	}
}

void
print_msr_data( struct msr_batch_array *a, int run, struct crypt *c ){
	static int init = 0;
	if(!init){
		init = 1;
		fprintf( stdout, "run HW cpu msrcmd err msr msrdata wmask aperf0 mperf0 aperf1 mperf1 msrdata1 cpu619 msrcmd619 err619 msr619 msrdata619 619aperf0 619mperf0 619aperf1 619mperf1 619msrdata1 cpu1b1 msrcmd1b1 err1b1 msr1b1 msrdata1b1 \n" );
	}
	for( uint64_t i=0; i<a->numops; i++ ){
		fprintf( stdout, 
			//hw	//cpu        msrcmd        err       msr           msrdata        wmask
			"%d" " %02"PRIu16 " %02"PRIu16" 0x%04"PRIx16" %"PRId32" 0x%08"PRIx32" %lf"" 0x%016"PRIx64
			//aperf0         mperf0         aperf1         mperf1         msrdata1
			" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" %lf"
		    //cpu619     //msrcmd619    //err619  //msr619     //msrdata619   
			" %02"PRIu16" 0x%04"PRIx16" %"PRId32" 0x%08"PRIx32" %lf"		
			//aperf0         mperf0         aperf1         mperf1         msrdata1
			" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" %lf"
		  	//cpu198     //msrcmd198    //err198  //msr198     //msrdata198   
			" %02"PRIu16" 0x%04"PRIx16" %"PRId32" 0x%08"PRIx32" 0x%016"PRIx64	
			//aperf0         mperf0         aperf1         mperf1         msrdata1
			" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016" PRIx64
			"\n",
			run,
			(int)(c->hw),
			(uint16_t)(a[0].ops[i].cpu),
			(uint16_t)(a[0].ops[i].msrcmd),
			( int32_t)(a[0].ops[i].err),
			(uint32_t)(a[0].ops[i].msr),
			(double)(a[0].ops[i].msrdata) / (1<<14),
			(uint64_t)(a[0].ops[i].wmask),
			(uint64_t)(a[0].ops[i].aperf0),
			(uint64_t)(a[0].ops[i].mperf0),
			(uint64_t)(a[0].ops[i].aperf1),
			(uint64_t)(a[0].ops[i].mperf1),
			(double)(a[0].ops[i].msrdata1) / (1<<14),
		
			(uint16_t)(a[1].ops[i].cpu),
			(uint16_t)(a[1].ops[i].msrcmd),
			( int32_t)(a[1].ops[i].err),
			(uint32_t)(a[1].ops[i].msr),
			(double)(a[1].ops[i].msrdata) / (1<<14),
			(uint64_t)(a[1].ops[i].aperf0),
			(uint64_t)(a[1].ops[i].mperf0),
			(uint64_t)(a[1].ops[i].aperf1),
			(uint64_t)(a[1].ops[i].mperf1),
			(double)(a[1].ops[i].msrdata1) / (1<<14),

			(uint16_t)(a[2].ops[i].cpu),
			(uint16_t)(a[2].ops[i].msrcmd),
			( int32_t)(a[2].ops[i].err),
			(uint32_t)(a[2].ops[i].msr),
			(uint64_t)(a[2].ops[i].msrdata),
			(uint64_t)(a[2].ops[i].aperf0),
			(uint64_t)(a[2].ops[i].mperf0),
			(uint64_t)(a[2].ops[i].aperf1),
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

#if 0
void
telemeter_init( struct msr_batch_array *a ){
	init_poll_energy( a );
	a[0].numops=2000;
	a[1].numops = 2000;
	a[2].numops = 2000;	// 1k per second, max is ONE_GiB/sizeof(msr_batch_array) ~ 15M.
}
#endif

void
per_instance_telemeter_init( struct msr_batch_array *a ){
	init_poll_energy( a );
	a[0].numops=2000;
	a[1].numops = 2000;
	a[2].numops = 2000;	// 1k per second, max is ONE_GiB/sizeof(msr_batch_array) ~ 15M.
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
per_instance_telemeter_finalize(struct msr_batch_array *a ){
	for(int i =0; i < 3; i++){
		free_GiB_pages(a[i].ops);
	}
		batch_ioctl( NULL );

	
}
void per_instance_payload_init(struct crypt *c){
	
	c[0].clear_text = allocate_GiB_pages( 1 );	
	c[0].encrypted_text = allocate_GiB_pages( 2 );
		// Initialize the random number generator.
		srandom( 13 );

		//should not  Randomize the cleartext buffer.
		memset(c[0].clear_text, 0, ONE_GiB );
		for(int i = 1; i < NUM_KEYS; i++){
			c[i].clear_text = c[0].clear_text;
			c[i].encrypted_text = c[0].encrypted_text;
				
		}		
		// NOTE:  Clarify if openssl is expecting a 256 bit or byte key
		for(int i = 0; i < NUM_KEYS; i++){	
			c[i].ctx = EVP_CIPHER_CTX_new();
			if( !(c[i].ctx) ){
				fprintf( stderr, "%s:%d EVP_CIPHER_CTX_new() failed, returned NULL.\n", __FILE__, __LINE__ );
				exit( -1 );
			}

			c[i].hw = i*100;
			keygen(c[i].key, KEY_LENGTH, c[i].hw);

			int rc = EVP_EncryptInit_ex( 
				c[i].ctx, 		// EVP_CIPHER_CTX*
				EVP_aes_256_ecb(),	// EVP_CIPHER*
				NULL,			// ENGINE*
				c[i].key,			// const unsigned char*
				NULL);

			if(1 != rc ){
				handleErrors();
			}
		}
	return;
}

void
per_instance_payload_finalize(struct crypt *c){
	free_GiB_pages(c[0].clear_text);
	free_GiB_pages(c[0].encrypted_text);
	for(int i = 0; i < NUM_KEYS; i++){ 	
		EVP_CIPHER_CTX_free( c[i].ctx );

	}
}
void per_run_payload_init(struct crypt *c){
	memset(c[0].encrypted_text, 0, ONE_GiB );
}

double
payload(struct crypt *c){
	int rc = 0;
	struct timeval start, stop;
	gettimeofday( &start, NULL );
//	for(int i = 0; i < 10; i++){
		 rc = EVP_EncryptUpdate(
			c->ctx,			// EVP_CIPHER_CTX*
			c->encrypted_text,	// unsigned char *out
			&(c->out_len),		// int *outl
			c->clear_text,		// const unsigned char *in
			(int)ONE_GiB);		// int inl

		if( !rc ){
			fprintf( stderr, "%s:%d EVP_EncryptUpdate() failed, returned 0.\n", __FILE__, __LINE__ );
			exit( -1 );
		}
//	}
	gettimeofday( &stop, NULL );
	return (stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec)/1000000.0;
}


int main(){
	int tid;
	struct msr_batch_array a[4] = {0};
	struct crypt c[NUM_KEYS];
	double elapsed_keys[NUM_KEYS];
	double elapsed_msr[3];
	int key_id = 0;
	int cpu_num;
	per_instance_telemeter_init(a);
	per_instance_payload_init(c);

	for(int runs = 0; runs < 10; runs++){	
			//per_run_telemeter_init(a);
			per_run_payload_init(c);

			omp_set_num_threads(4);
			//perhaps elapsed_msr should be shared in openmp pragma one param is which vars are priv or shared
			for(key_id = 0; key_id < NUM_KEYS; key_id++){  		
				#pragma omp parallel num_threads(3)
				{
					tid = omp_get_thread_num();
					cpu_num = sched_getcpu();
					switch( tid ){
						case 0: elapsed_msr[0] = telemeter( &(a[0]) );      break;
						case 1: elapsed_msr[1] = telemeter( &(a[1]) );      break;
	//					case 2: elapsed_msr[2] = telemeter( &(a[2]) );      break;
						case 2: elapsed_keys[key_id] = payload( &c[key_id] ); break;
						default: assert(0);                             	break;
					}		

       				fprintf(stderr, "Thread %3d is running on CPU %3d\n", tid, cpu_num);
				}
				for(int i = 0; i < 3; i++){
					fprintf( stderr, "%s:%d Telemeter thread %d elapsed seconds:  %lf\n", __FILE__, __LINE__, i, elapsed_msr[i] );
				}
				fprintf( stderr, "%s:%d Payload elapsed seconds:    %lf\n", __FILE__, __LINE__, elapsed_keys[key_id] );
				print_msr_data( a, runs, &c[key_id] );
				//per_run_telemeter_finalize( &c[key_id] );
			}
	}

	per_instance_telemeter_finalize(a);
	per_instance_payload_finalize(c);

	return 0;
}
