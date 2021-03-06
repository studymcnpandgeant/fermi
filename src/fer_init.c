/*

   Initialization FERMI routine 

   Authors:

   Guido Giuntoli
   Miguel Zavala

*/

#include "fermi.h"

int ferinit(int argc,char **argv)
{

  /* 

     Reads the input file
     Reads the mesh
     Reads the mesh
     Allocs mamory for K, x, b

   */

  int    ierr, i, d, *ghost;
  char   file_c[64];

  MPI_Init(&argc, &argv);

  //================================================== 
  // Read command line options 
  //================================================== 
  //
  // couple file if "-c" is not specified no coupling file is read
  //
  couple_fl = false;
  for(i=1; i<argc; i++){
    if(strcmp(argv[i],"-c")==0){
      couple_fl = true;
      if(i == argc - 1){
	return 1;
      }
      else{
	i++;
	strcpy(file_c,argv[i]);
      }
    }
  }

  //================================================== 
  // Stablish communicators
  //================================================== 
  //
  WORLD_Comm = MPI_COMM_WORLD;
  FERMI_Comm = WORLD_Comm;

  if( couple_fl == true ){
    // FERMI is going to be coupled with another code
    #ifdef COMMDOM 
      // so fills the "coupling" structure
      ierr = parse_coupling(file_c);
      if(ierr != 0){
        return 1;
      }
      coupling.INTER_Comm = malloc(coupling.num_friends * sizeof(MPI_Comm));

      ierr = fer_comm_init();
    #else
      // coupling NO WAY !
      PetscPrintf(FERMI_Comm,"fer_init.c: you have to link with "
	  "PLEPP for perming a coupled calculation.\n\n"); 
      return 1;
    #endif

  }

  PETSC_COMM_WORLD = FERMI_Comm;
  MPI_Comm_rank(FERMI_Comm, &local_rank);
  MPI_Comm_size(FERMI_Comm, &local_size);
  nproc = local_size;
  rank  = local_rank;

  // PETSC_COMM_WORLD has been set so it is possible to call
  // SlepcInitialize  or PetscInitialize (internally)
  SlepcInitialize(&argc,&argv,(char*)0,NULL);

  calcu.exec = (nproc>1)?PARALLEL:SEQUENCIAL;
  if(argc == 1){
    PetscPrintf(FERMI_Comm,"fer_init.c:input file NS.\n\n"); 
    return 1;
  }
  //
  //============================== 


  //============================== 
  // PARCING INPUT FILE
  //============================== 
  //    
  list_init(&list_mater, sizeof(pvl_t),    cmp_mat);
  list_init(&list_bound, sizeof(bound_t),  cmp_bou);
  list_init(&list_fun1d, sizeof(bound_t),  cmp_f1d);
  list_init(&list_ctrlr, sizeof(ctrlrod_t),NULL);
  list_init(&list_outpu, sizeof(output_t), NULL);
  list_init(&list_comms, sizeof(comm_t),   NULL);
  strcpy(inputfile,argv[1]);
  PetscPrintf(FERMI_Comm,"Parcing input file.\n");
  ierr=parse_input();
  if(ierr!=0)
  {
    PetscPrintf(FERMI_Comm,"fer_init.c:ierr parsing input file.\n");
    return 1;
  }
  //
  //============================== 


  //============================== 
  // READING MESH 
  //============================== 
  //   
  list_init(&list_nodes, sizeof(gmshN_t), gmsh_nodcmp);
  list_init(&list_ghost, sizeof(gmshN_t), gmsh_nodcmp);
  list_init(&list_elemv, sizeof(gmshE_t), cmp_int);
  list_init(&list_elems, sizeof(gmshE_t), cmp_int);
  list_init(&list_physe, sizeof(gmshP_t), cmp_int);    
  PetscPrintf(FERMI_Comm,"Reading mesh.\n");
  ierr=gmsh_read(meshfile,epartfile,npartfile,rank,DIM,&list_nodes,&list_ghost,&list_elemv,&list_elems,&list_physe,&loc2gold,&loc2gnew,&npp,nproc);    
  if(ierr!=0)
  {
    PetscPrintf(FERMI_Comm,"fer_init.c:ierr reading mesh.\n"); 
    return 1;
  }
  ntot=0;
  for(i=0;i<nproc;i++)
    ntot+=npp[i];

  // complete the volume element list inside each 
  // physical entity
  gmsh_phys_elmlist(&list_elemv,&list_physe); 
  //
  //============================== 


  //============================== 
  // OUTPUT INITIALIZATION
  //============================== 
  //    
  //  Here we open some file if the kind requiere them
  //
  node_list_t  * pn, * pp;
  output_t     * po;
  pn = list_outpu.head;
  while(pn){
    po = (output_t *)pn->data;
    switch (po->kind){

      case 1:
	break;

      case 2:
        // power on physical entities as a function of time 
	po->kind_2.fp = fopen(po->kind_2.file,"w");
	// we try to find the gmshid of the physical entities specified on input file
	// and save them on "ids" array inside kind_2
	for(i=0; i < po->kind_2.nphy; i++){
	  pp = list_physe.head;
	  while(strcmp(po->kind_2.phys[i],((gmshP_t*)pp->data)->name) != 0 ){
	    pp = pp->next;
	  }
	  if(pp != NULL){
	    po->kind_2.ids[i] = ((gmshP_t*)pp->data)->gmshid;
	  }
	  else{
	    PetscPrintf(FERMI_Comm,"Physical entity %s not found in gmsh file.\n",po->kind_2.phys[i]); 
	    return 1;
	  }
	}

	break;

      default:
	return 1;

    }
    pn = pn->next;
  }
  //
  //================================================================================ 

  //============================== 
  // COMMUNICATION INITIALIZATION
  //============================== 
  //    
  //  Prepare structures for stablishing communications
  //
  comm_t       * pcomm;
  pn = list_comms.head;
  while(pn){
    pcomm = (comm_t *)pn->data;
    switch (pcomm->kind){

      case 1:

	// we try to find the gmshid of the physical entities specified on input file
	// and save them on "ids" array inside "comm_1"
	for(i=0; i < pcomm->comm_1.nphy; i++){
	  pp = list_physe.head;
	  while(strcmp(pcomm->comm_1.phys[i],((gmshP_t*)pp->data)->name) != 0 ){
	    pp = pp->next;
	  }
	  if(pp != NULL){
	    pcomm->comm_1.ids[i] = ((gmshP_t*)pp->data)->gmshid;
	  }
	  else{
	    PetscPrintf(FERMI_Comm,"Physical entity %s not found in gmsh file.\n",pcomm->comm_1.phys[i]); 
	    return 1;
	  }
	}

	break;

      default:
	return 1;

    }
    pn = pn->next;
  }
  //
  //================================================================================ 

  //============================== 
  // PRINTING STRUCTURES
  //============================== 
  //     
  PetscPrintf(FERMI_Comm,"Printing structures 1.\n");
  ierr = print_struct(1);   
  if(ierr!=0){
    PetscPrintf(FERMI_Comm,"fer_init.c:ierr printing structures.\n"); 
    return 1;
  }
  //
  //============================== 

  //
  //============================== 
  // CONSTRUCTING MESH
  //============================== 
  //      
  PetscPrintf(FERMI_Comm,"Constructing mesh.\n");
  ierr=mesh_alloc(&list_nodes, &list_ghost, cpynode, &list_elemv, cpyelemv, &list_elems, cpyelems, &mesh);
  if(ierr) 
  {
    PetscPrintf(FERMI_Comm,"fer_init.c:ierr allocating mesh.\n"); 
    return 1;
  }
  ierr=mesh_renum(&mesh,loc2gold,loc2gnew);
  if(ierr)
  {
    PetscPrintf(FERMI_Comm,"fer_init.c:ierr renumbering mesh nodes.\n"); 
    return 1;
  }
  //
  //============================== 

  //      
  //==============================      
  // CONTROL RODS INIT
  //==============================      
  //      
  PetscPrintf(FERMI_Comm,"Initializing control rods.\n");
  ierr=ferirods();
  if(ierr)
  {
    PetscPrintf(FERMI_Comm,"fer_init.c:ierr control rods initialization.\n",ierr); 
    return 1;
  }
  //      
  //==============================      
  // ASSEMBLY BOUNDARIES 
  //==============================      
  //      
  PetscPrintf(FERMI_Comm,"Assemblying BCs.\n");
  ierr=ferbouset();
  if(ierr)
  {
    PetscPrintf(FERMI_Comm,"fer_init.c:ierr assembling BCs.\n"); 
    return 1;
  }
  //
  //============================== 
  
  //==============================      
  // ALLOCATING MATRICES/VECTORS 
  //==============================      
  //      
  PetscPrintf(FERMI_Comm,"Allocating Matrices/Vectors.\n");
  ghost=(int*)calloc(mesh.nghost*DIM,sizeof(int));
  for(i=0;i<mesh.nghost;i++)
  {
    for(d=0;d<DIM;d++)
      ghost[i*egn+d]=loc2gnew[mesh.nnodes+i]*egn+d;
  }

  VecCreateGhost(FERMI_Comm,mesh.nnodes*egn,ntot*egn,mesh.nghost*egn,(PetscInt*)ghost,&phi_n); 
  VecDuplicate(phi_n,&phi_o);
  VecDuplicate(phi_n,&b);
  VecDuplicate(phi_n,&b_a);
  MatCreateAIJ(FERMI_Comm,mesh.nnodes*egn,mesh.nnodes*egn,ntot*egn,ntot*egn,78,NULL,78,NULL,&A);
  MatCreateAIJ(FERMI_Comm,mesh.nnodes*egn,mesh.nnodes*egn,ntot*egn,ntot*egn,78,NULL,78,NULL,&B);
  MatCreateAIJ(FERMI_Comm,mesh.nnodes*egn,mesh.nnodes*egn,ntot*egn,ntot*egn,78,NULL,78,NULL,&M);
  MatCreateAIJ(FERMI_Comm,mesh.nnodes*egn,mesh.nnodes*egn,ntot*egn,ntot*egn,78,NULL,78,NULL,&K);

  Ae=(double*)calloc(NPE*egn*NPE*egn,sizeof(double));
  Be=(double*)calloc(NPE*egn*NPE*egn,sizeof(double));
  Me=(double*)calloc(NPE*egn*NPE*egn,sizeof(double));
  be=(double*)calloc(NPE*egn,sizeof(double));
  idxm=(int*)calloc(NPE*egn,sizeof(int));
  jac=(double**)calloc(DIM,sizeof(double*));
  for(i=0;i<DIM;i++)
    jac[i]=(double*)calloc(DIM,sizeof(double));
  ijac=(double**)calloc(DIM,sizeof(double*));
  for(i=0;i<DIM;i++)
    ijac[i]=(double*)calloc(DIM,sizeof(double));
  der=(double**)calloc(NPE,sizeof(double*));
  for(i=0;i<NPE;i++)
    der[i]=(double*)calloc(DIM,sizeof(double));
  coor=(double**)calloc(NPE,sizeof(double*));
  for(i=0;i<NPE;i++)
    coor[i]=(double*)calloc(DIM,sizeof(double));

  fem_inigau();
  if(ierr)
  {
    PetscPrintf(FERMI_Comm,"fer_init.c:ierr gps init.\n\n"); 
    return 1;
  }


  //==============================      
  // SETTING SOLVER
  //==============================      
  //      
  EPSCreate(FERMI_Comm,&eps);
  EPSSetProblemType(eps,EPS_GNHEP);    
  EPSSetOperators(eps,A,B);
  EPSSetWhichEigenpairs(eps,EPS_SMALLEST_MAGNITUDE);
  EPSSetType(eps,EPSJD);
  EPSSetDimensions(eps,1,PETSC_DEFAULT,PETSC_DEFAULT);
  EPSSetFromOptions(eps);

  KSPCreate(FERMI_Comm,&ksp);
  KSPSetType(ksp,KSPGMRES);
  KSPGetPC(ksp,&pc);
//  PCSetType(pc,PCLU);
  KSPSetFromOptions(ksp);
  PetscViewerASCIIOpen(FERMI_Comm,"kspinfo.dat",&kspview);
  KSPView(ksp,kspview);

 // EPSCreate(FERMI_Comm,&eps);
 // EPSSetProblemType(eps,EPS_GNHEP);

  //============================== 
  // PRINTING STRUCTURES
  //============================== 
  //     
  PetscPrintf(FERMI_Comm,"Printing structures 2.\n");
  ierr = print_struct(2);   
  if(ierr)
  {
    PetscPrintf(FERMI_Comm,"fer_init.c:ierr printing structures.\n"); 
    return 1;
  }
  return 0;
}
