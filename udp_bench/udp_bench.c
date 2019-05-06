/*
 * Copyright (c) 2015, University of Washington.
 *               2019, ETH Zürich
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. 
 * Attn: Systems Group.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <inttypes.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <strings.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>

#define	timersub(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;			      \
    if ((result)->tv_usec < 0) {					      \
      --(result)->tv_sec;						      \
      (result)->tv_usec += 1000000;					      \
    }									      \
  } while (0)

struct timeval st;

static pthread_t* threads; // all threads
static double* pkts_per_s; // total pkts per thread
static double* run_times; // all the end times of the threads
static int num_clients;
static char* if_name;
static int portno;
static double bench_run_time;
static int buf_size;
static char* ip;
static uint32_t max_pkts_in_flight = 1;
static uint32_t rounds;

// data of machine we send to
//static struct sockaddr_in clientaddr;
//static socklen_t clientlen;

static int setup_socket()
{
    int sockfd;
    int optval; /* flag value for setsockopt */

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("ERROR opening socket");
        exit(1);
    }

    /* setsockopt: Handy debugging trick that lets 
    * us rerun the server immediately after we kill it; 
    * otherwise we have to wait about 20 secs. 
    * Eliminates "ERROR on binding: Address already in use" error. 
    */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
             (const void *)&optval , sizeof(int));

    setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, if_name, strlen(if_name));

    return sockfd;
}

static void *client_func(void *arg)
{
    char buf[buf_size]; /* message buf */
    uint64_t* payload = (uint64_t*) &buf;
    uint64_t total_pkts = 0;
    uint64_t num_pkt_send = 0;
    uint64_t t_id = (uint64_t) arg;
    int sockfd;
    struct timeval diff;
    struct timeval end;
    int n;
    double time_s = 0;
    uint32_t pkts_in_flight = 0;
 
    struct sockaddr_in clientaddr;
    socklen_t clientlen;

    clientlen = sizeof(clientaddr);

    bzero((char *) &clientaddr, sizeof(clientaddr));
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_addr.s_addr = inet_addr(ip);
    if(clientaddr.sin_addr.s_addr == INADDR_NONE) {
      printf("Error on inet_addr()\n");
      exit(1);
    }
    clientaddr.sin_port = htons((unsigned short)portno);

    // setup socket including binding to interface
    sockfd = setup_socket();
 
    //printf("Thread %lu started using sockfd %d \n", t_id, sockfd);  
    //timersub(tstamp, &oldstamp, &diff);
    while (time_s < bench_run_time) {   

        // slowly start up with sending packets
        if (pkts_in_flight < max_pkts_in_flight) {
            payload[0] = num_pkt_send;
            n = sendto(sockfd, buf, buf_size, 0, (struct sockaddr *) &clientaddr, 
                   clientlen);
            if (n < 0) {
                printf("ERROR in sendto");
            }
            pkts_in_flight++;
            num_pkt_send++;
        }

        n = recvfrom(sockfd, buf, buf_size, MSG_DONTWAIT, NULL, NULL);
        if (n < 0) {
	    if (errno == EWOULDBLOCK || errno == EAGAIN) {
	    	
	    } else {
            	printf("ERROR in recvfrom");
            	pkts_in_flight--;
	    }
        }

        if (n == buf_size) {
            total_pkts++;
            // was a valid buffer, resend
            payload[0] = num_pkt_send;
            n = sendto(sockfd, buf, buf_size, 0, (struct sockaddr *) &clientaddr, 
                       clientlen);
            if (n < 0) {
                printf("ERROR in sendto");
            }
            num_pkt_send++;
        }

        gettimeofday(&end, NULL);
        timersub(&end, &st, &diff);
        time_s = ((double)diff.tv_sec * 1000000 + (double)diff.tv_usec)/1000000;
    }

    close(sockfd);
    run_times[t_id] = time_s; 
    pkts_per_s[t_id] = total_pkts/time_s;
    //printf("Thread %lu exit pkts recvd %lu \n", t_id, total_pkts);  
    return NULL;
}

int main(int argc, char **argv) 
{
    /* 
    * check command line arguments 
    */
    if (argc < 10) {
        fprintf(stderr, "usage: %s <num clients> <interface> <port> <server IP>" 
                        "<mesure time (in seconds)> <max packets in flight>"
                        "<buffer size> <number of rounds> <ouput file name>\n",
                         argv[0]);
        exit(1);
    }

    num_clients = atoi(argv[1]);
    if_name = argv[2]; // local interface name
    portno = atoi(argv[3]); // server port number
    ip = argv[4]; // server ip
    bench_run_time = atof(argv[5]); // run time of one round
    max_pkts_in_flight = atoi(argv[6]); // number of packets in flight
    buf_size = atoi(argv[7]); // buffer size
    rounds = atoi(argv[8]); // how many measurments of "bench_run_time" seconds
    char* out_file_name = argv[9];

    if ((num_clients >= 1) && (num_clients <= 100)) {
        pkts_per_s = (double*) calloc(num_clients, sizeof(double));
        run_times = (double*) calloc(num_clients, sizeof(double));
        threads = (pthread_t*) calloc(num_clients, sizeof(pthread_t));
    } else {
        fprintf(stderr, "Invalid client value. Should be 1 <= client value <= 100 \n");
        exit(1);
    }


    if (bench_run_time < 0) {
        fprintf(stderr, "Invalid run time. Should be > 0 \n");
        exit(1);
    }

    gettimeofday(&st, NULL);
    uint32_t num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    double tot_pkts_per_s = 0;
    FILE* file = fopen(out_file_name, "ab+");
    if (file == NULL) {
        fprintf(stderr, "Output file is invalid %s\n", out_file_name);
        exit(1);
    }
    fprintf(file, "############################################################"
                  "#################### \n");
    fprintf(file, "#Config: num_clients %d, if_name %s, portno %d, ip %s, \n" 
                  "#        bench_run_time %.2f (s), max_pkts_in_flight %d, buf_size %d,\n" 
                  "#        rounds %d, outfile %s \n",
            num_clients, if_name, portno, ip, bench_run_time, max_pkts_in_flight, 
            buf_size, rounds, out_file_name);
    fprintf(file, "Round \t | Pkts/s \t\t\t| Mbit/s incl. header \t\t| Mbit/s payload\n");
    for(uint32_t rnd = 0; rnd < rounds; rnd++) {
        for (uint64_t i = 0; i < (uint32_t) num_clients; i++) { 

        cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i % num_cores, &cpuset);

            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
        
            int ret = pthread_create(&threads[i], &attr, client_func, (void*) i);
            assert(ret == 0);

        }
        
        gettimeofday(&st, NULL);

        for (int i = 0; i < num_clients; i++) {
            pthread_join(threads[i], NULL);
        }
    
        tot_pkts_per_s = 0;
        for (int i = 0; i < num_clients; i++) {
            tot_pkts_per_s += pkts_per_s[i];
        }

        printf("Mbit/s %f including header \n", ((tot_pkts_per_s)*(buf_size+42)*8)/(1000*1000));
        printf("Mbit/s %f only payload \n", ((tot_pkts_per_s)*buf_size*8)/(1000*1000));
        printf("Pkts/s %f \n", tot_pkts_per_s);
        fprintf(file, "%d \t\t | %.2f \t\t| %.2f \t\t\t\t\t| %.2f\n", rnd, 
                tot_pkts_per_s,
                ((tot_pkts_per_s)*(buf_size+42)*8)/(1000*1000), 
                ((tot_pkts_per_s)*buf_size*8)/(1000*1000));
    }
    fprintf(file, "############################################################"
                  "#################### \n");

    fflush(file);
    fclose(file);

    return 0;
}
