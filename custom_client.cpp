/*
 * client.c
 * 
 * This file is the client program, 
 * which prepares the arguments, calls "rpcCall", and checks the returns.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rpc.h"
#include "messages.h"

#define CHAR_ARRAY_LENGTH 100

extern char * context_str;

int main() {
  char name_buffer[500];
  sprintf(name_buffer, "Client %d", getpid());
  context_str = name_buffer;

  //char * port = getenv ("BINDER_PORT");
  //char * address = getenv ("BINDER_ADDRESS");
  //print_with_flush(context_str, "Starting client with BINDER_ADDRESS: %s\n", address);
  //print_with_flush(context_str, "Starting client with BINDER_PORT: %s\n", port);

  /* prepare the arguments for f0 */
  int a0 = 5;
  int b0 = 10;
  int count0 = 3;
  int return0 = 0;
  int argTypes0[count0 + 1];
  void **args0;

  argTypes0[0] = (1 << ARG_OUTPUT) | (ARG_INT << 16);
  argTypes0[1] = (1 << ARG_INPUT) | (ARG_INT << 16);
  argTypes0[2] = (1 << ARG_INPUT) | (ARG_INT << 16);
  argTypes0[3] = 0;
    
  args0 = (void **)malloc(count0 * sizeof(void *));
  args0[0] = (void *)&return0;
  args0[1] = (void *)&a0;
  args0[2] = (void *)&b0;

  /* prepare the arguments for f1 */
  char a1 = 'a';
  short b1 = 100;
  int c1 = 1000;
  long d1 = 10000;
  int count1 = 5;
  long return1 = 0;
  int argTypes1[count1 + 1];
  void **args1;
    
  argTypes1[0] = (1 << ARG_OUTPUT) | (ARG_LONG << 16);
  argTypes1[1] = (1 << ARG_INPUT) | (ARG_CHAR << 16);
  argTypes1[2] = (1 << ARG_INPUT) | (ARG_SHORT << 16);
  argTypes1[3] = (1 << ARG_INPUT) | (ARG_INT << 16);
  argTypes1[4] = (1 << ARG_INPUT) | (ARG_LONG << 16);
  argTypes1[5] = 0;
    
  args1 = (void **)malloc(count1 * sizeof(void *));
  args1[0] = (void *)&return1;
  args1[1] = (void *)&a1;
  args1[2] = (void *)&b1;
  args1[3] = (void *)&c1;
  args1[4] = (void *)&d1;
    
  /* prepare the arguments for f2 */
  float a2 = 3.14159;
  double b2 = 1234.1001;
  int count2 = 3;
  char *return2 = (char *)malloc(CHAR_ARRAY_LENGTH * sizeof(char));
  memset(return2,0, CHAR_ARRAY_LENGTH * sizeof(char));
  int argTypes2[count2 + 1];
  void **args2;

  argTypes2[0] = (1 << ARG_OUTPUT) | (ARG_CHAR << 16) | CHAR_ARRAY_LENGTH;
  argTypes2[1] = (1 << ARG_INPUT) | (ARG_FLOAT << 16);
  argTypes2[2] = (1 << ARG_INPUT) | (ARG_DOUBLE << 16);
  argTypes2[3] = 0;

  args2 = (void **)malloc(count2 * sizeof(void *));
  args2[0] = (void *)return2;
  args2[1] = (void *)&a2;
  args2[2] = (void *)&b2;

  /* prepare the arguments for f3 */
  long a3[11] = {11, 109, 107, 105, 103, 101, 102, 104, 106, 108, 110};
  int count3 = 1;
  int argTypes3[count3 + 1];
  void **args3;

  argTypes3[0] = (1 << ARG_OUTPUT) | (1 << ARG_INPUT) | (ARG_LONG << 16) | 11;
  argTypes3[1] = 0;

  args3 = (void **)malloc(count3 * sizeof(void *));
  args3[0] = (void *)a3;

  /* prepare the arguemtns for f4 */
  const char *a4 = "non_exist_file_to_be_printed";
  char * lol = (char *)malloc(strlen(a4) + 1);
  memcpy(lol, a4, strlen(a4) + 1);
  int count4 = 1;
  int argTypes4[count4 + 1];
  void **args4;

  argTypes4[0] = (1 << ARG_INPUT) | (ARG_CHAR << 16) | strlen(lol);
  argTypes4[1] = 0;

  args4 = (void **)malloc(count4 * sizeof(void *));
  args4[0] = (void *)lol;

  /* rpcCalls */
  int s0 = rpcCall((char *)"fun0", argTypes0, args0);
  if(s0){
        print_with_flush(context_str, "Failure in test f0.  Expected %d return code, actual %d.\n", 0, s0);
  }
  if ((a0 + b0) != *((int *)(args0[0]))) { 
    print_with_flush(context_str, "failure in f0 Expected %d, got %d.\n", (a0 + b0) != *((int *)(args0[0])));
  }
  free(args0);


  int s1 = rpcCall((char *)"fun1", argTypes1, args1);
  if(s1){
        print_with_flush(context_str, "Failure in test f1.  Expected %d return code, actual %d.\n", 0, s1);
  }
  if ((a1 + b1 * c1 - d1) != *((long *)(args1[0]))) { 
    print_with_flush(context_str, "failure in f1 Expected %ld, got %ld: \n", (a1 + b1 * c1 - d1) ,*((long *)(args1[0])));
  }
  free(args1);


  int s2 = rpcCall((char *)"fun2", argTypes2, args2);
  const char result2[] = "31234";
  if(s2){
        print_with_flush(context_str, "Failure in test f2.  Expected %d return code, actual %d.\n", 0, s2);
  }
  if (strcmp(result2, (char *)args2[0])) {
    print_with_flush(context_str, "f2 failure. Expected %s got %s.\n", result2, (char *)args2[0]);
  }
  free(args2);


  int s3 = rpcCall((char *)"fun3", argTypes3, args3);
  if(s3){
        print_with_flush(context_str, "Failure in test f3.  Expected %d return code, actual %d.\n", 0, s3);
  }

  long results[] = {110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 11};

  int i;
  for (i = 0; i < 11; i++) {
    if(results[i] != *(((long *)args3[0]) + i)){
        print_with_flush(context_str, "Failure in test f3.  Expected %d, actual %d.\n", results[i], *(((long *)args3[0]) + i));
    }
  }
  free(args3);

  int s4 = rpcCall((char *)"fun4", argTypes4, args4);
  free(args4);
  if(!(s4)){
      print_with_flush(context_str, "f4 was expecting a non zero response code.  Expected %s, got %d\n","(definitely not 0)", s4);
  }

  /* rpcTerminate */
  //print_with_flush(context_str, "\ndo you want to terminate? y/n: ");
  //if (getchar() == 'y')
  free(return2);
  free(lol);

  /* end of client.c */
  return 0;
}




