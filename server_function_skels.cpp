#include "server_functions.h"
#include "messages.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

int f0_Skel(int *argTypes, void **args) {
  printf("Server successfully called f0_skel.\n");
  fflush(stdout);
  struct function_prototype f = create_function_prototype((char*)"", argTypes);
  assert(get_num_args_inputs(f) == 2);
  assert(get_num_args_outputs(f) == 1);
  *((int*)get_nth_args_array(f, args, 0, ARG_OUTPUT)) = f0(*((int*)get_nth_args_array(f, args, 0, ARG_INPUT)), *((int*)get_nth_args_array(f, args, 1, ARG_INPUT)));
  free(f.arg_data);
  return 0;
}

int f1_Skel(int *argTypes, void **args) {
  printf("Server successfully called f1_skel.\n");
  fflush(stdout);
  return 0;
}

int f2_Skel(int *argTypes, void **args) {
  printf("Server successfully called f2_skel.\n");
  fflush(stdout);
  return 0;
}

int f3_Skel(int *argTypes, void **args) {
  printf("Server successfully called f3_skel.\n");
  fflush(stdout);
  return 0;
}

/* 
 * this skeleton doesn't do anything except returns
 * a negative value to mimic an error during the 
 * server function execution, i.e. file not exist
 */
int f4_Skel(int *argTypes, void **args) {
  printf("Server successfully called f4_skel.\n");
  fflush(stdout);
  return -1; /* can not print the file */
}

