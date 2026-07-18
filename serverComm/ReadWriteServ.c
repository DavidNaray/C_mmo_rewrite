

#include "ReadWriteServ.h"
#include "../../DistributedNodes/scheduler.h"

void send_message(char* msg){
    DWORD bytesWritten=0;
    DWORD bytesToWrite = (DWORD)(strlen(msg));
    
    BOOL ok = WriteFile(
        scheduler.msgPipe,
        msg,
        bytesToWrite,
        &bytesWritten,
        NULL
    );

    if (!ok){printf("WriteFile failed: %lu\n", GetLastError());}

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