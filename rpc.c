#include <stdio.h>
#include "rpc.h"

int rpcInit(){
    printf("rpcInit has not been implemented yet.\n");
    return -1;
};

int rpcCall(char* name, int* argTypes, void** args){
    printf("rpcCall has not been implemented yet.\n");
    return -1;
};

int rpcCacheCall(char* name, int* argTypes, void** args){
    printf("rpcCacheCall has not been implemented yet.\n");
    return -1;
};

int rpcRegister(char* name, int* argTypes, skeleton f){
    printf("rpcRegister has not been implemented yet.\n");
    return -1;
};

int rpcExecute(){
    printf("rpcExecute has not been implemented yet.\n");
    return -1;
};

int rpcTerminate(){
    printf("rpcTerminate has not been implemented yet.\n");
    return -1;
};
