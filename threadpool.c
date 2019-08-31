#include "threadpool.h"

void destroy_threadpool(threadpool* destroyme);
void freeAll(threadpool* freeMe);
int print_int(void *x);

threadpool* create_threadpool(int num_threads_in_pool){
    threadpool* newThread;
    if(num_threads_in_pool>MAXT_IN_POOL || num_threads_in_pool<0){
        //usage;
        return NULL;
    }
    else{
        int rc;
        newThread = (threadpool*)malloc(sizeof(threadpool));
        if(newThread==NULL){
            printf("malloc error");
            return NULL; // check
        }
        newThread->num_threads = num_threads_in_pool;
        newThread->qsize = 0;
        newThread->shutdown = 0;
        newThread->dont_accept = 0;
        newThread->qhead = NULL;
        newThread->qtail = NULL;
        if(pthread_mutex_init(&newThread->qlock,NULL)){
            free(newThread);
            perror("pthread_mutex_init()error");
            return NULL;
        }
        if(pthread_cond_init(&newThread->q_not_empty,NULL)){
            pthread_mutex_destroy(&newThread->qlock);
            free(newThread);
            perror("pthread_mutex_init()error");
            return NULL;
        }
        if(pthread_cond_init(&newThread->q_empty,NULL)){
            pthread_mutex_destroy(&newThread->qlock);
            pthread_cond_destroy(&newThread->q_not_empty);
            free(newThread);
            perror("pthread_mutex_init()error");
            return NULL; 
        }
        newThread->threads = (pthread_t*)malloc(sizeof(pthread_t)*newThread->num_threads);
        if(newThread->threads==NULL){
            pthread_mutex_destroy(&newThread->qlock);
            pthread_cond_destroy(&newThread->q_not_empty);
            pthread_cond_destroy(&newThread->q_empty);
            free(newThread);
        }
        for(int i=0;i<newThread->num_threads;i++){
            rc = pthread_create(&newThread->threads[i],NULL,do_work,(void*)newThread);
            if(rc){
                printf("pthread_create()error");
                freeAll(newThread);
                return NULL;//check
            }
        }
        return newThread;
    }
}


void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    pthread_mutex_lock(&from_me->qlock);
    if(from_me->dont_accept==0){
        work_t* curWork = (work_t*)malloc(sizeof(work_t));
        if(curWork==NULL){
            printf("malloc error\n");
            pthread_mutex_unlock(&from_me->qlock);
            freeAll(from_me);
            return;
        }
        curWork->arg = arg;
        curWork->routine = dispatch_to_here;
        curWork->next = NULL;

        if(from_me->qhead==NULL){
            from_me->qhead = curWork;
            from_me->qtail = curWork;
        }
        else{
            (from_me->qtail)->next = curWork;
            from_me->qtail = (from_me->qtail)->next;
        }
        from_me->qsize++;
        pthread_mutex_unlock(&from_me->qlock);
        if(pthread_cond_signal(&from_me->q_not_empty)!=0){
            freeAll(from_me);
            perror("pthread_cond_signal()error");
            exit(1);
        }
    }
    else{
        pthread_mutex_unlock(&from_me->qlock);
        return;
    }
    free(curWork);
}

void* do_work(void* p){
    threadpool* curThread = (threadpool*)p;
    while(true){
        pthread_mutex_lock(&curThread->qlock);
        if(curThread->shutdown==1){
            pthread_mutex_unlock(&curThread->qlock);
            pthread_exit(NULL);
        }
        if(curThread->qsize==0){
            if(pthread_cond_wait(&curThread->q_not_empty, &curThread->qlock)!=0){
            freeAll(curThread);
            perror("pthread_cond_wait()error");
            exit(1);
            }
        }
        if(curThread->shutdown==1){
            pthread_mutex_unlock(&curThread->qlock);
            pthread_exit(NULL);
        }
        
        work_t* curWork ;
        if(curThread->qhead!=NULL)
         curWork = curThread->qhead;
     
        else{
            pthread_mutex_unlock(&curThread->qlock);
            continue;
        }
     
        curThread->qhead = curThread->qhead->next;
        curThread->qsize--;
        pthread_mutex_unlock(&curThread->qlock);
        if(curWork){
        curWork->routine(curWork->arg);
        }
        if(curThread->qsize==0 && curThread->dont_accept==1){
            pthread_mutex_unlock(&curThread->qlock);
            if(pthread_cond_signal(&curThread->q_empty)!=0){
              freeAll(curThread);
              perror("pthread_cond_signal()error");
              exit(1);              
            }
        }
        free(curWork);
    }
}


void destroy_threadpool(threadpool* destroyme){
    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept=1;
    if(destroyme->qsize>0){
        if(pthread_cond_wait(&destroyme->q_empty,&destroyme->qlock)!=0){
            freeAll(destroyme);
            perror("pthread_cond_wait()error");
            exit(1);//check
        }
    }
    destroyme->shutdown = 1;
    pthread_mutex_unlock(&destroyme->qlock);
    pthread_cond_broadcast(&destroyme->q_not_empty);// check error
    for(int i=0;i<destroyme->num_threads;i++){
        if(pthread_join(destroyme->threads[i], NULL)!=0){
            freeAll(destroyme);
            perror("pthread_cond_wait()error");
            exit(1);//check            
        }
    }
    freeAll(destroyme);
}

void freeAll(threadpool* freeMe){
            pthread_mutex_destroy(&freeMe->qlock);
            pthread_cond_destroy(&freeMe->q_not_empty);
            pthread_cond_destroy(&freeMe->q_empty);
            free(freeMe->threads);
            free(freeMe);
}

int print_int(void *x){
    int *temp =((int*) x);
    printf("im thread %d\n",*temp);
    return 0;
}


