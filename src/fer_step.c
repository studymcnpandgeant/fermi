/* Newton - Raphson iterations*/

#include "fermi.h"

int ferstep_ST(void)
{

//  double norm;
  
  if(fersrods(calcu.t)){
    PetscPrintf(PETSC_COMM_WORLD,"Problem calculating control rods.\n"); 
    return 1;
  }
  if(ferass_ST()){
    PetscPrintf(PETSC_COMM_WORLD,"Problem assembling the A & B."); 
    return 1;
  }
  if(fersolv_ST()){
    PetscPrintf(PETSC_COMM_WORLD,"Problem solving the steady state.\n"); 
    return 1;
  }
//  VecNorm(phi_n,NORM_2,&norm);
  /* Normalize the flux according to the power */
  if(fernorm()){
    PetscPrintf(PETSC_COMM_WORLD,"Problem normalizing the power.\n"); 
    return 1;
  }

  return 0;
}

int ferstep_TR(int step)
{

  if(fersrods(calcu.t)){
    PetscPrintf(PETSC_COMM_WORLD,"Problem calculating control rods.\n"); 
    return 1;
  }
  if(ferass_TR(step)){
    PetscPrintf(PETSC_COMM_WORLD,"Problem assembling the A & b.\n"); 
    return 1;
  }
  if(fersolv_TR()){
    PetscPrintf(PETSC_COMM_WORLD,"Problem solving the transient steps.\n"); 
    return 1;
  }

  return 0;
}