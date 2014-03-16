#include <stdio.h>
#include <unistd.h>
#include "rpc.h"
#include "server_function_skels.h"
#include "global_state.h"
#include <unistd.h>

/*  Declared in global_state.h */
extern int server_to_binder_sockfd;
extern const char * context_str;

int main(int argc, char *argv[]) {
  char name_buffer[500];
  sprintf(name_buffer, "Server %d", getpid());
  context_str = name_buffer;
  /* create sockets and connect to the binder */
  rpcInit();

  /* prepare server functions' signatures */
  int count0 = 3;
  int count1 = 5;
  int count2 = 3;
  int count3 = 1;
  int count4 = 1;
  int argTypes0[count0 + 1];
  int argTypes1[count1 + 1];
  int argTypes2[count2 + 1];
  int argTypes3[count3 + 1];
  int argTypes4[count4 + 1];

  argTypes0[0] = (1 << ARG_OUTPUT) | (ARG_INT << 16);
  argTypes0[1] = (1 << ARG_INPUT) | (ARG_INT << 16);
  argTypes0[2] = (1 << ARG_INPUT) | (ARG_INT << 16);
  argTypes0[3] = 0;

  argTypes1[0] = (1 << ARG_OUTPUT) | (ARG_LONG << 16);
  argTypes1[1] = (1 << ARG_INPUT) | (ARG_CHAR << 16);
  argTypes1[2] = (1 << ARG_INPUT) | (ARG_SHORT << 16);
  argTypes1[3] = (1 << ARG_INPUT) | (ARG_INT << 16);
  argTypes1[4] = (1 << ARG_INPUT) | (ARG_LONG << 16);
  argTypes1[5] = 0;

  /* 
   * the length in argTypes2[0] doesn't have to be 100,
   * the server doesn't know the actual length of this argument
   */
  argTypes2[0] = (1 << ARG_OUTPUT) | (ARG_CHAR << 16) | 100;
  argTypes2[1] = (1 << ARG_INPUT) | (ARG_FLOAT << 16);
  argTypes2[2] = (1 << ARG_INPUT) | (ARG_DOUBLE << 16);
  argTypes2[3] = 0;

  /*
   * f3 takes an array of long. 
  */
  argTypes3[0] = (1 << ARG_OUTPUT) | (1 << ARG_INPUT) | (ARG_LONG << 16) | 11;
  argTypes3[1] = 0;

  /* same here, 28 is the exact length of the parameter */
  argTypes4[0] = (1 << ARG_INPUT) | (ARG_CHAR << 16) | 28;
  argTypes4[1] = 0;

  /* 
   * register server functions f0~f4
   */
  rpcRegister((char *)"f0", argTypes0, *f0_Skel);
  rpcRegister((char *)"f1", argTypes1, *f1_Skel);
  rpcRegister((char *)"f2", argTypes2, *f2_Skel);
  rpcRegister((char *)"f3", argTypes3, *f3_Skel);
  rpcRegister((char *)"f4", argTypes4, *f4_Skel);

  /* call rpcExecute */
  rpcExecute();

  //printf("Server closing socket definition '%d' to binder\n", server_to_binder_sockfd);
  /*  Close the connection that we created in rpcInit */
  close(server_to_binder_sockfd);

  /* return */
  return 0;
}







