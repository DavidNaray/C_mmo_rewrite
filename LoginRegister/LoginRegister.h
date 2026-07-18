#ifndef LogReg_H
#define LogReg_H   // these form a guard

typedef struct {
    int RId;
    char username[256];
    char password[128];
}RegisterArgs;

void RegisterTask(void *arg);

void LoginTask(void *arg);

#endif