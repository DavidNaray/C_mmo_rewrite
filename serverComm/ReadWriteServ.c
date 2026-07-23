

#include "ReadWriteServ.h"
#include "../../DistributedNodes/scheduler.h"

void send_message(char* msg){
    if (msg == NULL) return;

    size_t orig_len = strlen(msg);
    // Allocate space for the original string plus the '\n' character
    size_t framed_len = orig_len + 1; 
    
    char* framed_msg = malloc(framed_len);
    if (framed_msg == NULL) {
        printf("send_message: malloc failed\n");
        return;
    }

    // Copy original data and swap the null terminator for a newline
    memcpy(framed_msg, msg, orig_len);
    framed_msg[orig_len] = '\n'; 

    DWORD bytesWritten = 0;
    DWORD bytesToWrite = (DWORD)framed_len;
    
    BOOL ok = WriteFile(
        scheduler.msgPipe,
        framed_msg,       // Write our newly framed message
        bytesToWrite,
        &bytesWritten,
        NULL
    );

    if (!ok) {
        printf("WriteFile failed: %lu\n", GetLastError());
    }

    // Always free what you temporarily allocate!
    free(framed_msg); 
}

void setupPipe(){
    scheduler.hPipe = CreateNamedPipe(
        TEXT("\\\\.\\pipe\\SchedulerPipe"),//simply the name of the pipe
        PIPE_ACCESS_DUPLEX,//two way communication
        
        // stream of messages, read from it as messages, and its blocking so read/write 
            //of it dont return immediately
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 
        1,//nMaxInstances
        65536,//nOutBufferSize
        65536,//nInBufferSize
        0,//nDefaultTimeOut
        NULL//[in, optional] LPSECURITY_ATTRIBUTES lpSecurityAttributes
    );
    scheduler.msgPipe = CreateNamedPipe(
        TEXT("\\\\.\\pipe\\SchedulerPipeMessage"),//simply the name of the pipe
        PIPE_ACCESS_DUPLEX,//two way communication
        
        // stream of messages, read from it as messages, and its blocking so read/write 
            //of it dont return immediately
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 
        1,//nMaxInstances
        65536,//nOutBufferSize
        65536,//nInBufferSize
        0,//nDefaultTimeOut
        NULL//[in, optional] LPSECURITY_ATTRIBUTES lpSecurityAttributes
    );
}