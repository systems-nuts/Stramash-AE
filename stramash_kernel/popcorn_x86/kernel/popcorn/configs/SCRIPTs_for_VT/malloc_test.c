#include <stdio.h>
#include <sys/time.h> 
#include "migrate.h"

int main(int argc, char *argv[])
{
	unsigned long i, ptr, t1, t2, t3, t4;
	unsigned long *buff;
	struct timeval tvalBefore, tvalAfter; 
//origin allocated origin memory 400MB
	buff = (unsigned long*)malloc(1024*1024*100*sizeof(unsigned long));
	ptr = (unsigned long)buff;
	for(i = 0; i< 1024*1024*100; i++)
		buff[i] = ptr;
	
//origin access origin allocation
///////////////////////////////////////
	//count time1 >>
	gettimeofday (&tvalBefore, NULL);
	for(i = 0; i< 1024*1024*100; i++)
                buff[i] = buff[i] - i;
	//count time1 <<
	gettimeofday (&tvalAfter, NULL);
	t1= ((tvalAfter.tv_sec - tvalBefore.tv_sec)*1000000L+tvalAfter.tv_usec) - tvalBefore.tv_usec;
///////////////////////////////////////

//remote access origin allocation
///////////////////////////////////////
	//count time2 >>
	gettimeofday (&tvalBefore, NULL);
        migrate(1, 0, 0);
        for(i = 0; i< 1024*1024*100; i++)
                buff[i] = buff[i] + i;
        migrate(0, 0, 0);
	//count time2 <<
	gettimeofday (&tvalAfter, NULL);
	t2= ((tvalAfter.tv_sec - tvalBefore.tv_sec)*1000000L+tvalAfter.tv_usec) - tvalBefore.tv_usec;
////////////////////////////////////////
        if(buff[1024*1024*100-1] == ptr)
                printf("test1 match\n");

	free(buff);


//remote allocated remote memory 400MB
	migrate(1, 0, 0);
        buff = (unsigned long*)malloc(1024*1024*100*sizeof(unsigned long));
        ptr = (unsigned long)buff;
        for(i = 0; i< 1024*1024*100; i++)
                buff[i] = ptr;
	migrate(0, 0, 0);


//remote access remote allocation
////////////////////////////////////
	//count time3 >>
	gettimeofday (&tvalBefore, NULL);
	migrate(1, 0, 0);
        for(i = 0; i< 1024*1024*100; i++)
                buff[i] = buff[i] + i;
	//count time3 <<
	migrate(0, 0, 0);
	gettimeofday (&tvalAfter, NULL);
	t3= ((tvalAfter.tv_sec - tvalBefore.tv_sec)*1000000L+tvalAfter.tv_usec) - tvalBefore.tv_usec;
/////////////////////////////////////



//origin access remote allocation
///////////////////////////////////
	//count time4 >>
	gettimeofday (&tvalBefore, NULL);
        for(i = 0; i< 1024*1024*100; i++)
                buff[i] = buff[i] - i;
	//count time4 <<
	gettimeofday (&tvalAfter, NULL);
	t4= ((tvalAfter.tv_sec - tvalBefore.tv_sec)*1000000L+tvalAfter.tv_usec) - tvalBefore.tv_usec;
////////////////////////////////////

        if(buff[1024*1024*100-1] == ptr)
                printf("test2 match\n");
	migrate(1, 0, 0);
	free(buff);
        migrate(0, 0, 0);	
	printf("origin access origin %ld\nremote access origin %ld\nremote access remote %ld\norigin access remote %ld\n", t1,t2,t3,t4);
	
	return 0;
}
