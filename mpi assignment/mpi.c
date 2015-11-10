#include <stdio.h>
#include <stdlib.h>
#include <time.h>		/* Used to seed the random number generator */
#include <mpi.h>




/*
	Serial quicksort in-place. You should call the routine "serialQuicksort()" (below) as:
	
	serialQuickSort( list, start, end );
	
	with
	
	float *list;			List to be sorted
	int start;				Start index (initially 0; becomes non-zero during recursion)
	int end;				End index (i.e. the list size, if start=0).

*/	
int serialPartition( float *list, int start, int end, int pivotIndex )
{
	/* Scratch variables */
	float temp;

	/* Get the pivot value */
	float pivotValue = list[pivotIndex];

	/* Move pivot to the end of the list (or list segment); swap with the value already there. */
	list[pivotIndex] = list[end-1];
	list[end-1]      = pivotValue;

	/* Sort into sublists using swaps; get all values below the pivot to the start of the list */
	int storeIndex = start, i;					/* storeIndex is where the values less than the pivot go */
	for( i=start; i<end-1; i++ )		/* Don't loop over the pivot (currently at end-1); will handle that just before returning */
		if( list[i] <= pivotValue )
		{
			/* Swap value with this element and that at the current storeIndex */
			temp = list[i]; list[i] = list[storeIndex]; list[storeIndex] = temp;

			/* Increment the store index; will point to one past the pivot index at the end of this loop */
		storeIndex++;			/* Could do this at the end of the previous instruction */
	}

	/* Move pivot to its final place */
	temp = list[storeIndex]; list[storeIndex] = list[end-1]; list[end-1] = temp;

	return storeIndex;
}




/* Performs the quicksort (partitioning and recursion) */
void serialQuicksort( float *list, int start, int end )
{
	if( end > start+1 )
	{
		int pivotIndex = start;				/* Choice of pivot index */

		/* Partition and return list of pivot point */
		int finalPivotIndex = serialPartition( list, start, end, pivotIndex );

		/* Recursion on sublist smaller than the pivot (including the pivot value itself) ... */
		serialQuicksort( list, start, finalPivotIndex );

		/* .. and greater than the pivot value (not including the pivot) */
		serialQuicksort( list, finalPivotIndex+1, end );
	}
	/* else ... only had one element anyway; no need to do anything */
}

void displayFullList( float *list, int n )
{
	
	/* Do not display large lists */
	if( n>100 )
	{
		printf( "Not displaying 'full list' - n too large.\n" );
		return;
	}
		
	int i;
	for( i=0; i<n; i++ ) printf( " %g", list[i] );
	printf( "\n" );
}


/*
	Main
*/
int main( int argc, char **argv )
{
		
	int numprocs, rank, i, n, rc;
	double start_time, end_time;

	/* Replace these lines with the corresponding MPI routines */
	rc = MPI_Init(&argc,&argv);
	if (rc != MPI_SUCCESS){
     		printf ("Error starting MPI program. Terminating.\n");
     		MPI_Abort(MPI_COMM_WORLD, rc);
     	}

	MPI_Comm_size (MPI_COMM_WORLD,&numprocs);
	MPI_Comm_rank (MPI_COMM_WORLD,&rank);
	
	
	/* Get n from the command line. Note that you should always finalise MPI before quitting. */
	if( argc != 2 ){
		if( rank==0 ) printf( "Need one argument (= the list size n).\n" );
		return EXIT_FAILURE;
	}

	n = atoi( argv[1] );

	if( n<=0 ){
		if( rank==0 ) printf( "List size must be >0.\n" );
		return EXIT_FAILURE;
	}
		
	if(rank == 0)
		printf( "\nN Procs = %d  Array size = %d\n\n",numprocs,n);
  
		
	start_time = MPI_Wtime();
	
	
	float *globalArray = NULL;		/* Initial unsorted list, and final sorted list; only on rank==0 */	
	float *short_bucket[numprocs];

	
	
	if( rank==0 ){
		MPI_Status status;
		int j;
		int bucket_sizes[numprocs];			// Size of each bucket
		srand( time(NULL) );				// Seed the random number generator to the system time
		
		globalArray = (float*) malloc( sizeof(float)*n );
		float bucket_lim = 1.0 / numprocs;		//Pivot 
		
		/* Create an array of size n and fill it with random numbers*/
		for( i=0; i<n; i++ ) 
			globalArray[i] = 1.0*rand() / RAND_MAX;

		printf("Full list: ");
		displayFullList(globalArray,n);

		/*Alloc memory for buckets and initialize the size on 0*/
		for(i = 0; i < numprocs; i++){
			short_bucket[i] = (float*) malloc( sizeof(float)* n ); 
			bucket_sizes[i] = 0;	
		}
		
		/* Partitionate the global array into the bukets*/
		for( i=0; i<n; i++ ){
			for(j=0; j<numprocs; j++){
				if((globalArray[i] > (bucket_lim * j)) && (globalArray[i] <= (bucket_lim * (j+1)))){
					short_bucket[j][bucket_sizes[j]] = globalArray[i];
					bucket_sizes[j]++;
				}
			}
		} 
		
		/*Send buckets to each process*/
		for(i=1; i<numprocs; i++){
			MPI_Send( &bucket_sizes[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD );
			MPI_Send( short_bucket[i], bucket_sizes[i], MPI_FLOAT, i, 0, MPI_COMM_WORLD );
		}
		
		/*Sort bucket 0*/
		serialQuicksort(short_bucket[0],0,bucket_sizes[0]);


		/*Receive Sorted buckets from each process*/
		for(i=1; i<numprocs; i++){
		  MPI_Recv(short_bucket[i], bucket_sizes[i], MPI_FLOAT, i, 0, MPI_COMM_WORLD, &status );
		}

		
		/*Concatenate sorted buckets*/		
		int index = 0;
		for(i=0; i<numprocs; i++){
			for(j=0; j< bucket_sizes[i]; j++){
				globalArray[index++] = short_bucket[i][j];							
			}
		}

		printf("Final sorted List: ");
		displayFullList(globalArray,n);
	
	}
	/*Rank != 0*/
	else{
		int size;
		
		MPI_Status status;
		
		/*Receive the size of bucket*/
		MPI_Recv( &size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status );

		/*Alloc memory and receive the bucket*/
		float *bucket = (float*) malloc( sizeof(float)*size );		
		MPI_Recv( bucket, size, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, &status );


		printf("Procces %d:  bucket size %d.  Elemets: ",rank,size);
		displayFullList(bucket,size);

		serialQuicksort(bucket,0,size);
		printf("Procces %d:  Sorted Elemets: ",rank);
		displayFullList(bucket,size);

		MPI_Send( bucket, size, MPI_FLOAT, 0, 0, MPI_COMM_WORLD);
	}
	
	
	

	end_time = MPI_Wtime();

	if(rank == 0)
		printf("Total time: %f secs\n",end_time - start_time);


	MPI_Finalize();

	return 0;

}
