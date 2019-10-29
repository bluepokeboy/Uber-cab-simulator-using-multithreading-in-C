#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>
#include<time.h>
#include<sys/shm.h>
#include<errno.h>
#include<semaphore.h>
// Structures
struct riders{
     int index;
     int cab_type;
     int max_wait_time;
     int ride_time;
     int arrival_time;
     int status;
} * riders;

struct cab{
     int index;
     int state;
} * cab;

struct servers{
     int index;
     int status;
} * servers;

// Global Variables, Mutex, Semaphores
pthread_mutex_t *mutex_cabs,*mutex_servers,*mutex_riders;
int n,m,k,number_cabs;
int riderflag[10000];
int riderflag2[10000];
sem_t cabs_sem;
sem_t server_sem;

// Checking for new cab when pooling
void * singlepool(void * arg){
     int *index_pointer = (int*) arg;
     int index = *index_pointer;
     int s;
     struct timespec ts;
     if(clock_gettime(CLOCK_REALTIME, &ts) == -1){
          return NULL;
     }
     ts.tv_sec += riders[index].max_wait_time;
     while((s = sem_timedwait(&cabs_sem, &ts)) == -1 && errno == EINTR){
          continue;
     }
     int flag=0;
     if(riderflag[index]==1){//found onepool
          flag=1;
          if(s!=-1){
               sem_post(&cabs_sem);
               return NULL;
          }
     }
     if (s != -1){
          riderflag[index]=2;//found cab
     }
     else{
          riderflag[index]=-1;
          if (errno == ETIMEDOUT){//found nothing
               printf("Rider %d could not find a cab\n",riders[index].index);
          }
          else{
               perror("sem_timedwait");
          }
     }
}
//Booking a cab
void BookCab(int cab_type, int max_wait_time, int ride_time, int index){
     sleep(riders[index].arrival_time);
     printf("Rider %d is looking for a cab\n",riders[index].index);
     // Premier
     if(cab_type==0){
          int s;
          struct timespec ts;
          if(clock_gettime(CLOCK_REALTIME, &ts) == -1){
               return;
          }
          ts.tv_sec += riders[index].max_wait_time;
          while((s = sem_timedwait(&cabs_sem, &ts)) == -1 && errno == EINTR){
               continue;
          }
          if (s == -1){
               if (errno == ETIMEDOUT){
                    printf("Rider %d could not find a cab\n",riders[index].index);
               }
               else{
                    perror("sem_timedwait");
               }
          }
          else{
               int riding=-1;
               for(int i=0; i<n; i++){
                    if(pthread_mutex_trylock(&mutex_cabs[i])){
                         continue;
                    }
                    if(cab[i].state==0){
                         cab[i].state=1;
                         riding=i;
                         printf("Rider %d is riding in cab %d with state %d\n",riders[index].index,cab[i].index,cab[i].state);
                         pthread_mutex_unlock(&mutex_cabs[i]);
                         break;
                    }
                    pthread_mutex_unlock(&mutex_cabs[i]);
               }
               if(riding!=-1){
                    sleep(riders[index].ride_time);
                    cab[riding].state=0;
                    printf("Rider %d finished cab ride with cab %d with state %d\n",riders[index].index,cab[riding].index,cab[riding].state);
                    sem_post(&cabs_sem);
                    riders[index].status=1;
                    printf("Rider %d is ready to pay\n",riders[index].index);
                    sem_post(&server_sem);
               }
               riding=-1;
          }
     }
     else{
          int riding=-1;
          int j;
          for(j=0;j<n; j++){
               if(pthread_mutex_trylock(&mutex_cabs[j])){
                    continue;
               }
               if(cab[j].state==3){
                    cab[j].state=2;
                    riding=j;
                    printf("Rider %d is riding in cab %d with state %d\n",riders[index].index,cab[j].index,cab[j].state);
                    pthread_mutex_unlock(&mutex_cabs[j]);
                    break;
               }
               pthread_mutex_unlock(&mutex_cabs[j]);
          }
          if(riding!=-1){
               sleep(riders[index].ride_time);
               pthread_mutex_lock(&mutex_cabs[riding]);
               if(cab[riding].state==2){
                    cab[riding].state=3;
               }
               else{
                    sem_post(&cabs_sem);
                    cab[riding].state=0;
               }
               printf("Rider %d finished cab ride with cab %d with state %d\n",riders[index].index,cab[riding].index,cab[riding].state);
               pthread_mutex_unlock(&mutex_cabs[riding]);
               sem_post(&cabs_sem);
               riders[index].status=1;
               printf("Rider %d is ready to pay\n",riders[index].index);
               sem_post(&server_sem);
               riding=-1;
               return;
          }
          if(j>=n){
               pthread_t riderspool;
               pthread_create(&riderspool,NULL,singlepool,(void *)&riders[index].index);
               int potentialcabindex=-1;
               int flag=0;
               while(riderflag[index]==0){
                    for(int i=0; i<n; i++){
                         if(pthread_mutex_trylock(&mutex_cabs[i])){
                              continue;
                         }
                         if(cab[i].state==3){
                              potentialcabindex=i;
                              riderflag[index]=1;
                              cab[i].state=2;
                              break;
                         }
                         pthread_mutex_unlock(&mutex_cabs[i]);
                         if(riderflag[index]!=0){
                              break;
                         }
                    }
               }
               int lol=potentialcabindex;
               if(riderflag[index]==2){
                    if(potentialcabindex!=-1){
                         cab[potentialcabindex].state=3;
                         pthread_mutex_unlock(&mutex_cabs[potentialcabindex]);
                    }
                    for(int i=0; i<n; i++){
                         if(pthread_mutex_trylock(&mutex_cabs[i])){
                              continue;
                         }
                         if(cab[i].state==0){
                              potentialcabindex=i;
                              cab[i].state=3;
                              pthread_mutex_unlock(&mutex_cabs[i]);
                              break;
                         }
                         pthread_mutex_unlock(&mutex_cabs[i]);
                    }
               }
               if(lol!=-1){
                    pthread_mutex_unlock(&mutex_cabs[lol]);
               }
               else if(riderflag[index]==-1){
                    pthread_join(riderspool,NULL);
                    return;
               }
               printf("Rider %d is riding in cab %d with state %d\n",riders[index].index,cab[potentialcabindex].index, cab[potentialcabindex].state);
               sleep(riders[index].ride_time);
               pthread_mutex_lock(&mutex_cabs[potentialcabindex]);
               if(cab[potentialcabindex].state==2){
                    cab[potentialcabindex].state=3;
               }
               else{
                    sem_post(&cabs_sem);
                    cab[potentialcabindex].state=0;
               }
               pthread_mutex_unlock(&mutex_cabs[potentialcabindex]);
               //sem_post(&cab);
               printf("Rider %d finished cab ride with cab %d with state %d\n",riders[index].index,cab[potentialcabindex].index, cab[potentialcabindex].state);
               //pthread_mutex_lock(&mutex_riders[index]);
               riders[index].status=1;
               printf("Rider %d is ready to pay\n",riders[index].index);
               //pthread_mutex_unlock(&mutex_riders[index]);
               sem_post(&server_sem);
               pthread_join(riderspool,NULL);
               return;
          }
     }
}

void * rider_init(void * arg){
     int *index_pointer = (int*) arg;
     int index = *index_pointer;
     // 0 is premier 1 is pool
     riders[index].arrival_time=(rand()%10);
     riders[index].cab_type=(rand()%2);
     riders[index].max_wait_time=5+(rand()%5);
     riders[index].ride_time=10+(rand()%10);
     riders[index].status=0;
     printf("Rider %d has arrival time %d, seeks cab_type %d, has max_wait_time %d, ride_time of %d\n",riders[index].index,riders[index].arrival_time, riders[index].cab_type,riders[index].max_wait_time,riders[index].ride_time);
     BookCab(riders[index].cab_type,riders[index].max_wait_time,riders[index].ride_time,index);
}

void * server_init(void * arg){
     int *index_pointer = (int*) arg;
     int index = *index_pointer;
     servers[index].status=0;
     printf("Server %d is now online\n",servers[index].index);
     while(1){
          servers[index].status=0;
          sem_wait(&server_sem);
          printf("Server %d has detected a user who is done with his ride and ready for payment\n",servers[index].index);
          servers[index].status=1;
          for(int i=0; i<m ;i++){
               pthread_mutex_lock(&mutex_riders[i]);
               if(riders[i].status==1){
                    printf("Server %d is accepting the payment of rider %d\n",servers[index].index,riders[i].index);
                    sleep(2);
                    riders[i].status=2;
                    printf("Server %d has accepted the payment of rider %d\n",servers[index].index,riders[i].index);
                    pthread_mutex_unlock(&mutex_riders[i]);
                    break;
               }
               pthread_mutex_unlock(&mutex_riders[i]);
          }
     }
}

int main(void){
     srand(time(NULL));
     scanf("%d %d %d",&n,&m,&k);
     //printf("lol\n");
     mutex_cabs=(pthread_mutex_t *)malloc((n)*sizeof(pthread_mutex_t));
     mutex_servers=(pthread_mutex_t *)malloc((k)*sizeof(pthread_mutex_t));
     mutex_riders=(pthread_mutex_t *)malloc((m)*sizeof(pthread_mutex_t));
     cab=(struct cab*)malloc(sizeof(struct cab)*(n));
     for(int i=0; i<n; i++){
          //printf("lol\n");
          cab[i].index=i;
          cab[i].state=0;
          //printf("%d\n",cab[i].index);
     }
     riders=(struct riders*)malloc(sizeof(struct riders)*(m));
     for(int i=0; i<n; i++){
          riderflag2[i]=0;
     }
     for(int i=0; i<n; i++){
          riderflag[i]=0;
     }
     //printf("lol\n");
     number_cabs=n;
     int ridersremain=m;
     //printf("lol\n");
     sem_init(&cabs_sem,0,number_cabs);
     sem_init(&server_sem,0,0);
     //printf("lol\n");
     // cab_init();
     servers=(struct servers*)malloc(sizeof(struct servers)*(k));
     pthread_t serverstids[k];
     for(int i=0; i<k; i++){
          servers[i].index=i;
          usleep(100);
          pthread_create(&serverstids[i],NULL,server_init,(void *)&servers[i].index);
     }
     sleep(1);
     //printf("lol\n");
     riders=(struct riders*)malloc(sizeof(struct riders)*(m));
     pthread_t riderstids[m];
     //printf("lol\n");
     for(int i=0; i<m; i++){
          riders[i].index=i;
          usleep(100);
          //printf("%d\n",riders[i].index);
          pthread_create(&riderstids[i],NULL,rider_init,(void *)&riders[i].index);
     }
     for(int i=0; i<m; i++){
          pthread_join(riderstids[i],NULL);
     }
     printf("Simulation Over.\n");
     return 0;
}
