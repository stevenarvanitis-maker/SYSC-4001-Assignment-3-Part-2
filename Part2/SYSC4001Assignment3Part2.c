// 
// @author Steven Arvanitis 101303797
// @author

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define MAX_QS 5

typedef struct {
    char rubric[MAX_QS];
    int student_id;
    int question_done[MAX_QS];
    int current_exam;
    int num_exams;
    int finished;
} Shared;

static void rand_sleep(double min_s, double max_s) {
    double r = (double)rand() / RAND_MAX;
    double s = min_s + r * (max_s - min_s);
    useconds_t us = (useconds_t)(s * 1000000.0);
    usleep(us);
}

static void load_rubric(Shared *shm, const char *rubric_file) {
    FILE *f = fopen(rubric_file, "r");
    char line[128];
    int num;
    char ch;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%d , %c", &num, &ch) == 2) {
            shm->rubric[num - 1] = ch;
        }
    }
    fclose(f);
}

static void save_rubric(Shared *shm, const char *rubric_file) {
    FILE *f = fopen(rubric_file, "w");
    for (int i = 0; i < MAX_QS; i++) {
        fprintf(f, "%d, %c\n", i + 1, shm->rubric[i]);
    }
    fclose(f);
}

static void load_exam(Shared *shm, int idx, char *exam_files[]) {
    if (idx >= shm->total_exams) {
        shm->finished = 1;
        return;
    }

    const char *name = exam_files[idx];
    FILE *f = fopen(name, "r");
    char line[128];

    fgets(line, sizeof(line), f);
    fclose(f);

    shm->student_id = atoi(line);
    shm->current_exam = idx;

    for (int i = 0; i < MAX_QS; i++) {
        shm->question_done[i] = 0;
    }

    if (shm->student_id == 9999) {
        shm->finished = 1;
    }

    printf("[INFO] Loaded exam %s (student %04d)\n", name, shm->student_id);
    fflush(stdout);
}

static void ta_process(int id, Shared *shm, char *exam_files[], const char *rubric_file) {
    srand(time(NULL) ^ getpid());

    while (!shm->finished) {
        for (int q = 0; q < MAX_QS && !shm->finished; q++) {
            printf("TA %d: reading rubric for Q%d (now '%c')\n", id, q + 1, shm->rubric[q]);
            fflush(stdout);

            rand_sleep(0.5, 1.0);

            if (rand() % 4 == 0) {
                char before = shm->rubric[q];
                if (before >= 'A' && before <= 'Y') {
                    shm->rubric[q] = before + 1;
                }
                char after = shm->rubric[q];

                printf("TA %d: correcting rubric Q%d: %c -> %c\n", id, q + 1, before, after);
                fflush(stdout);

                save_rubric(shm, rubric_file);
            }
        }

        while (!shm->finished) {
            int q_index = -1;

            for (int i = 0; i < MAX_QS; i++) {
                if (shm->question_done[i] == 0) {
                    q_index = i;
                    shm->question_done[i] = 1;
                    break;
                }
            }

            if (q_index == -1) {
                int next = shm->current_exam + 1;
                load_exam(shm, next, exam_files);
                break;
            }

            int q_num = q_index + 1;
            int student = shm->student_id;

            printf("TA %d: marking student %04d question %d\n", id, student, q_num);
            fflush(stdout);

            rand_sleep(1.0, 2.0);

            printf("TA %d: finished student %04d question %d\n", id, student, q_num);
            fflush(stdout);
        }
    }

    printf("TA %d: exiting\n", id);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    int num_TAs = atoi(argv[1]);
    const char *rubric_file = argv[2];
    int total_exams = argc - 3;

    int shmid = shmget(IPC_PRIVATE, sizeof(Shared), IPC_CREAT | 0666);
    Shared *shm = (Shared *)shmat(shmid, NULL, 0);

    memset(shm, 0, sizeof(Shared));
    shm->total_exams = total_exams;

    char **exam_files = &argv[3];

    load_rubric(shm, rubric_file);
    load_exam(shm, 0, exam_files);

    for (int i = 0; i < num_TAs; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            ta_process(i, shm, exam_files, rubric_file);
            shmdt(shm);
            exit(0);
        }
    }

    for (int i = 0; i < num_TAs; i++) {
        wait(NULL);
    }

    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);

    printf("All TAs finished");
    return 0;
}
