#include <stdint.h>  // uint32
#include <stdlib.h>  // exit, perror
#include <unistd.h>  // usleep
#include <stdio.h>   // printf
#include <pthread.h> // pthread_*
#include "queue.c"
#include "sleep.c"
#include <sys/mman.h>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
typedef dispatch_semaphore_t psem_t;
#else
#include <semaphore.h> // sem_*
typedef sem_t psem_t;
#define true 1
#define false 0
#endif
void psem_init(psem_t *sem, uint32_t value);
void psem_wait(psem_t *sem);
void psem_post(psem_t *sem);

int willAnswer(double probability);

// Make the program a bit slower
#define SLOWDOWN usleep(1000)
int total_question;
int commentator;
int question;
double probability;
int max_speak;
double break_prob;
int answer_count = 0;
int cn = 0;
int selected_id = -1;
struct Queue *queue;
int program_finished = 0;
int breaking_event = 0;
struct timeval stop, start;
long minute, second, milisecond;
char *miliS, *secondS, *minuteS;
 
time_t seconds;
psem_t question_asked;
psem_t select_comm;
psem_t mutex;
psem_t next_question;
psem_t *you_are_selected;
psem_t *finished_speaking;
psem_t program_mtx;

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t condmtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cond1mtx = PTHREAD_MUTEX_INITIALIZER;
void init()
{
    // TODO : code here
    // psem_init(&ask_question, 0);
    psem_init(&question_asked, 0);
    psem_init(&select_comm, 0);
    psem_init(&mutex, 1);
    psem_init(&next_question, 1);
    psem_init(&program_mtx, 1);
    for (size_t i = 0; i < commentator; i++)
    {
        psem_init(&you_are_selected[i], 0);
        psem_init(&finished_speaking[i], 0);
    }
}
// Acquire a lock
void lock(psem_t *sem)
{
    // TODO : code here
    psem_wait(sem);
}

// Release an acquired lock
void unlock(psem_t *sem)
{
    // TODO : code here
    psem_post(sem);
}
void findtimediff()
{
    long sec, msec, tmilli;
    gettimeofday(&stop, NULL);
    sec = (stop.tv_sec - start.tv_sec);
    msec = (stop.tv_usec - start.tv_usec);

    tmilli = (1000 * sec) + (msec * 0.001);
    minute = tmilli / 60000;
    second = (tmilli % 60000) / 1000;
    milisecond = (tmilli % 60000) % 1000;

    if (second < 10)
    {
        sprintf(secondS, "0%ld", second);
    }
    else
    {
        sprintf(secondS, "%ld", second);
    }
    if (milisecond < 10)
    {
        sprintf(miliS, "00%ld", milisecond);
    }
    else if (milisecond < 100)
    {
        sprintf(miliS, "0%ld", milisecond);
    }
    else
    {
        sprintf(miliS, "%ld", milisecond);
    }
    if (minute< 10)
    {
       sprintf(minuteS, "0%ld", minute);
    }else{
       sprintf(minuteS, "%ld", minute); 
    }
    
}
void *ask()
{
    while (true)
    {
        /* code */
        lock(&program_mtx);
        pthread_mutex_lock(&cond1mtx);
        while (breaking_event == 1)
        {
            pthread_cond_wait(&cond1, &cond1mtx);
        }
        pthread_mutex_unlock(&cond1mtx);
        if (question == 0)
        {
            question -= 1;
            program_finished = 1;
            unlock(&program_mtx);
            unlock(&question_asked);
            break;
        }
        unlock(&program_mtx);
        question -= 1;
        findtimediff();
        printf("[%s:%s:%s] Moderator asked question no: %d \n", minuteS, secondS, miliS, total_question - question);
        unlock(&question_asked);

        lock(&select_comm);
        lock(&question_asked);
        if (queue->size == 0)
        {
            findtimediff();
            printf("[%s:%s:%s] Nobody wants to answer question no: %d \n", minuteS, secondS, miliS, total_question - question);
        }

        while (!isEmpty(queue))
        {
            selected_id = dequeue(queue);
            pthread_mutex_lock(&cond1mtx);
            while (breaking_event == 1)
            {
                pthread_cond_wait(&cond1, &cond1mtx);
            }
            pthread_mutex_unlock(&cond1mtx);
            unlock(&you_are_selected[selected_id]);
            lock(&finished_speaking[selected_id]);
        }
        for (size_t i = 0; i < commentator; i++)
        {
            /* code */
            unlock(&you_are_selected[i]);
        }
    }
    findtimediff();
    printf("[%s:%s:%s] Moderator is leaving\n", minuteS, secondS, miliS);
    return NULL;
}

int willAnswer(double probability)
{
    return (rand() % 100) <= probability * 100;
}
void *answer(int id)
{
    while (true)
    {
        lock(&mutex);
        cn += 1;
        if (cn == 1)
        {
            lock(&question_asked);
        }
        lock(&program_mtx);
        if (program_finished == 1)
        {
            unlock(&mutex);
            unlock(&program_mtx);
            break;
        }
        unlock(&program_mtx);
        int have_answer = willAnswer(probability);
        if (have_answer == 1)
        {
            findtimediff();
            printf("[%s:%s:%s] Commentator #%d  generates answer, position in queue: %d\n", minuteS, secondS, miliS, id, queue->size);
            enqueue(queue, id);
        }
        if (cn == commentator)
        {
            unlock(&question_asked);
            unlock(&select_comm);
            cn = 0;
        }
        unlock(&mutex);
        if (have_answer == 1)
        {
            lock(&you_are_selected[id]);
            findtimediff();
            printf("[%s:%s:%s] Commentator #%d is selected \n", minuteS, secondS, miliS, id);
            double num = (double)rand() / (double)(RAND_MAX / max_speak);
            findtimediff();
            printf("[%s:%s:%s] Commentator #%d's turn to speak for %f seconds.\n", minuteS, secondS, miliS, id, num);

            struct timespec timetoexpire;
            struct timeval tp;
            //When to expire is an absolute time, so get the current time and add //it to our delay time
            gettimeofday(&tp, NULL);
            long new_nsec = tp.tv_usec * 1000 + (num - (long)num) * 1e9;
            timetoexpire.tv_sec = tp.tv_sec + (long)num + (new_nsec / (long)1e9);
            timetoexpire.tv_nsec = new_nsec % (long)1e9;
            pthread_mutex_lock(&condmtx);
            while (breaking_event == 0)
            {
                pthread_cond_timedwait(&cond, &condmtx, &timetoexpire);
                if (breaking_event == 0)
                {
                    pthread_mutex_unlock(&condmtx);
                    findtimediff();
                    printf("[%s:%s:%s] Commentator #%d's finished speaking. \n", minuteS, secondS, miliS, id);
                    break;
                }
                else
                {
                    pthread_mutex_unlock(&condmtx);
                    findtimediff();
                    printf("[%s:%s:%s] Commentator #%d' is cut short due to a breaking news\n", minuteS, secondS, miliS, id);
                    pthread_mutex_lock(&cond1mtx);
                    pthread_cond_wait(&cond1, &cond1mtx);
                    pthread_mutex_unlock(&cond1mtx);
                    break;
                }
            }
            have_answer = 0;
            unlock(&finished_speaking[id]);
        }

        lock(&you_are_selected[id]);
    }
    findtimediff();
    printf("[%s:%s:%s] Commentator #%d is leaving. \n", minuteS, secondS, miliS, id);
    return 0;
}

void *handle_event()
{
    while (true)
    {
        lock(&program_mtx);
        if (program_finished == 0)
        {
            unlock(&program_mtx);
            pthread_mutex_lock(&condmtx);
            while (breaking_event == 0)
            {
                pthread_cond_wait(&cond, &condmtx);
            }
            findtimediff();
            printf("[%s:%s:%s] Breaking news!\n", minuteS, secondS, miliS);
            pthread_mutex_unlock(&condmtx);
            pthread_mutex_lock(&cond1mtx);
            while (breaking_event == 1)
            {
                pthread_cond_wait(&cond1, &cond1mtx);
            }
            findtimediff();
            printf("[%s:%s:%s] Breaking news ends\n", minuteS, secondS, miliS);
            pthread_mutex_unlock(&cond1mtx);
        }
        else
        {
            unlock(&program_mtx);

            break;
        }
    }
    findtimediff();
    printf("[%s:%s:%s] Event handler is leaving\n", minuteS, secondS, miliS);
    return NULL;
}

int mycmd(int argc, char *argv[], int *commentator, int *questions, double *prob, int *time, double *breaking_p)
{
    int i = 1;

    // parse commandline arguments.
    while (i < argc)
    {
        if (strcmp(argv[i], "-n") == 0)
        {
            *commentator = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-q") == 0)
        {
            *questions = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            *prob = atof(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-t") == 0)
        {

            *time = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            *breaking_p = atof(argv[i + 1]);
        }
        i++;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    commentator = 3;
    total_question = 5;
    probability = 0.5;
    max_speak = 3;
    break_prob = 0.2;
    mycmd(argc, argv, &commentator, &total_question, &probability, &max_speak, &break_prob);
    question = total_question;
    miliS = (char *)malloc(3 * sizeof(char));
    secondS = (char *)malloc(3 * sizeof(char));
    minuteS = (char *)malloc(3 * sizeof(char));
    printf("commentator = %d, questions = %d, probability = %f, max_speak = %d, breakprob = %f \n", commentator, question, probability, max_speak, break_prob);
    you_are_selected = mmap(NULL, sizeof(you_are_selected) * commentator, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    finished_speaking = mmap(NULL, sizeof(finished_speaking) * commentator, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    init();
    srand(time(0));
    pthread_t tid[5] = {0};
    pthread_t moderator;
    pthread_t event_handler;
    queue = createQueue(1000);

    gettimeofday(&start, NULL);

    for (size_t i = 0; i < commentator; i++)
    {
        pthread_create(&tid[i], NULL, answer, i);
    }
    pthread_create(&moderator, NULL, ask, NULL);
    pthread_create(&event_handler, NULL, handle_event, NULL);
    while (true)
    {
        lock(&program_mtx);
        if (program_finished == 0)
        {
            unlock(&program_mtx);
            pthread_mutex_lock(&condmtx);
            breaking_event = willAnswer(break_prob);
            if (breaking_event == 1)
            {
                pthread_cond_broadcast(&cond);
                pthread_mutex_unlock(&condmtx);

                pthread_sleep(5.0);
                pthread_mutex_lock(&cond1mtx);
                breaking_event = 0;
                pthread_cond_broadcast(&cond1);
                pthread_mutex_unlock(&cond1mtx);
            }
            else
            {
                pthread_mutex_unlock(&condmtx);
            }

            pthread_sleep(1.0);
        }
        else
        {
            unlock(&program_mtx);
            
            break;
        }
        unlock(&program_mtx);
    }

    
    pthread_join(moderator, NULL);
    // pthread_join(event_handler, NULL);
    for (int i = 0; i < commentator; ++i)
    {
        pthread_join(tid[i], NULL);
    }

    return 0;
}

// psem_* functions are sem_* alternatives compatible on both macOs and Linux.
#ifdef __APPLE__
void psem_init(psem_t *sem, uint32_t value)
{
    *sem = dispatch_semaphore_create(value);
}
void psem_wait(psem_t *sem)
{
    dispatch_semaphore_wait(*sem, DISPATCH_TIME_FOREVER);
}
void psem_post(psem_t *sem)
{
    dispatch_semaphore_signal(*sem);
}
#else
void psem_init(psem_t *sem, u_int32_t value)
{
    sem_init(sem, 0, value);
}
void psem_wait(psem_t *sem)
{
    sem_wait(sem);
}
void psem_post(psem_t *sem)
{
    sem_post(sem);
}
#endif
